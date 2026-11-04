import hashlib,json
from pathlib import Path
root=Path('/build/sqlite3-3.40.1')
p=root/'src/where.c'
before=p.read_bytes();s=before.decode()
assert hashlib.sha256(before).hexdigest()=='7ef061e8cb2d2bcfda2cb7153536f7b47bae055bb6c42e2f7de7e5292e12d991','Unexpected Debian source preimage'
old='''        pWInfo->eDistinct = WHERE_DISTINCT_ORDERED;
      }
    }else{'''
new='''        pWInfo->eDistinct = WHERE_DISTINCT_ORDERED;
      }
      if( pWInfo->pSelect->pOrderBy
       && pWInfo->nOBSat > pWInfo->pSelect->pOrderBy->nExpr ){
        pWInfo->nOBSat = pWInfo->pSelect->pOrderBy->nExpr;
      }
    }else{'''
assert s.count(old)==1,'Upstream context changed'
assert (root/'debian/changelog').read_text().startswith('sqlite3 (3.40.1-2+deb12u2)')
after=s.replace(old,new).encode();p.write_bytes(after)
Path('/build/backport-receipt.json').write_text(json.dumps({'cve':'CVE-2025-7458','debianSource':'3.40.1-2+deb12u2','fossil':'12ad822d9b827777526ca5ed5bf3e678d600294fc9b5c25482dfff2a021328a4','git':'b816ca9994e03a8bc829b49452b8158a731e81a9','beforeSha256':hashlib.sha256(before).hexdigest(),'afterSha256':hashlib.sha256(after).hexdigest()},indent=2))
