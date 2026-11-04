import json,os,sqlite3,tempfile,unittest
from pathlib import Path

if os.environ.get('NUMFORGE_SQLITE_BACKPORT_CHECK') != '1' or not Path('/.dockerenv').is_file():
    raise RuntimeError('Run only in the bounded disposable SQLite qualification container.')

class Checks(unittest.TestCase):
    def test_upstream_distinct_order_by(self):
        with sqlite3.connect(':memory:') as db:
            for columns,orders in [(66,64),(100,64),(200,199),(66,67)]:
                query='SELECT DISTINCT '+','.join(['1']*columns)+' ORDER BY '+','.join(["'x'"]*orders)
                self.assertEqual(db.execute(query).fetchall(),[tuple([1]*columns)])
                self.assertEqual(db.execute('EXPLAIN '+query).fetchone()[1],'Init')
            db.execute('CREATE TABLE t AS '+query)
            self.assertEqual(db.execute('SELECT count(*) FROM t').fetchone(),(1,))
    def test_values_transactions_foreign_keys(self):
        db=sqlite3.connect(':memory:');self.addCleanup(db.close)
        db.execute('pragma foreign_keys=on');db.executescript('create table p(id integer primary key, v text,b blob); create table c(id references p(id));')
        db.execute('insert into p values(?,?,?)',(1,'výpočet 😀',b'\x00\xff'));db.commit()
        self.assertEqual(db.execute('select v,b from p').fetchone(),('výpočet 😀',b'\x00\xff'))
        with self.assertRaises(sqlite3.IntegrityError):db.execute('insert into c values(99)')
        db.rollback();db.execute('delete from p');db.rollback();self.assertEqual(db.execute('select count(*) from p').fetchone(),(1,))
    def test_features(self):
        with sqlite3.connect(':memory:') as db:
            self.assertEqual(db.execute("select json_extract('{\"x\":4}','$.x'),sqrt(16)").fetchone(),(4,4.0))
            db.execute('create virtual table f using fts5(x)');db.execute("insert into f values('hello world')")
            self.assertEqual(db.execute("select count(*) from f where f match 'hello'").fetchone(),(1,))
            db.execute('create virtual table r using rtree(id,x0,x1)');db.execute('insert into r values(1,0,2)')
            self.assertEqual(db.execute('select id from r where x0<=1 and x1>=1').fetchone(),(1,))
    def test_wal_backup_and_readonly_restore(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'source.db';q=Path(folder)/'copy.db';a=sqlite3.connect(p);b=sqlite3.connect(p)
            try:
                self.assertEqual(a.execute('pragma journal_mode=wal').fetchone(),('wal',))
                a.execute('create table t(x)');a.execute('insert into t values(42)');a.commit()
                self.assertEqual(b.execute('select x from t').fetchone(),(42,))
                with sqlite3.connect(q) as dest:a.backup(dest)
                c=sqlite3.connect(q.as_uri()+'?mode=ro',uri=True)
                try:
                    self.assertEqual(c.execute('pragma integrity_check').fetchone(),('ok',));self.assertEqual(c.execute('select x from t').fetchone(),(42,))
                    with self.assertRaises(sqlite3.OperationalError):c.execute('delete from t')
                finally:c.close()
            finally:a.close();b.close()

unittest.main(verbosity=2)
