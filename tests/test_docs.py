"""Check documentation staging, bounded output replacement and navigation."""
import importlib.util
from pathlib import Path
import re
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_docs', ROOT / 'scripts/build_docs.py')
docs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(docs)


class DocumentationStagingTests(unittest.TestCase):
    def setUp(self):
        self.base = ROOT / 'build'
        self.base.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='numforge-docs-', dir=self.base)
        self.root = Path(self.temp.name).resolve()
        self.assertEqual(self.root.parent, self.base.resolve())
        for name in docs.EXTRA_PAGES:
            self.write(name, '# Example\n')
        self.write('docs/README.md', '# Guide\n')
        self.write('LICENSE', 'License\n')
        self.output = self.root / 'build/docs/content'

    def tearDown(self):
        self.assertEqual(self.root.parent, self.base.resolve())
        self.assertTrue(self.root.name.startswith('numforge-docs-'))
        self.temp.cleanup()

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding='utf-8')

    def test_stages_original_pages_and_only_linked_resources(self):
        page = '# Čísla\n[API](reference/api.md#rule)\n![plot](images/plot%20one.png)\n<img src="images/logo.png">\n'
        self.write('docs/README.md', page)
        self.write('docs/reference/api.md', '# API\n## Rule\n[Example](../../examples/example.c)\n')
        self.write('docs/images/plot one.png', 'image')
        self.write('docs/images/logo.png', 'logo')
        self.write('examples/example.c', 'int main(void) { return 0; }\n')
        self.write('unrelated.txt', 'not a documentation input')
        docs.prepare_sources(self.root, self.output)
        self.assertEqual((self.output / 'docs/README.md').read_text(encoding='utf-8'), page)
        self.assertTrue((self.output / 'docs/images/plot one.png').is_file())
        self.assertTrue((self.output / 'docs/images/logo.png').is_file())
        self.assertTrue((self.output / 'examples/example.c').is_file())
        self.assertFalse((self.output / 'unrelated.txt').exists())
        self.assertEqual((self.root / 'docs/README.md').read_text(encoding='utf-8'), page)

    def test_missing_link_is_an_error(self):
        self.write('docs/README.md', '[Missing](missing.md)\n')
        with self.assertRaisesRegex(ValueError, 'missing linked input'):
            docs.prepare_sources(self.root, self.output)

    def test_checkout_escape_is_an_error(self):
        self.write('docs/README.md', '[Outside](../../outside.txt)\n')
        with self.assertRaisesRegex(ValueError, 'escapes the checkout'):
            docs.prepare_sources(self.root, self.output)

    def test_invalid_utf8_is_an_error(self):
        (self.root / 'docs/README.md').write_bytes(b'# Bad\x97')
        with self.assertRaises(UnicodeDecodeError):
            docs.prepare_sources(self.root, self.output)

    def test_cleanup_replaces_only_dedicated_generated_directories(self):
        docs.clean_generated(self.root, self.output)
        (self.output / 'old.txt').write_text('generated')
        docs.clean_generated(self.root, self.output)
        self.assertEqual(list(self.output.iterdir()), [])
        for target in (self.root, self.root / 'docs', self.root / 'build/docs/personal'):
            with self.assertRaisesRegex(ValueError, 'non-documentation output'):
                docs.clean_generated(self.root, target)
        self.assertEqual((self.root / 'README.md').read_text(), '# Example\n')


class DocumentationNavigationTests(unittest.TestCase):
    def test_each_document_has_one_navigation_entry(self):
        # Our explicit nav uses plain relative paths, without YAML aliases/macros.
        entries = re.findall(r':\s*(\S+\.md)\s*$', (ROOT / 'mkdocs.yml').read_text(), re.M)
        navigation = set(entries)
        self.assertEqual(len(entries), len(navigation), 'duplicate canonical navigation entry')
        for entry in navigation:
            self.assertTrue((ROOT / entry).is_file(), f'{entry}: missing navigation target')
        supplementary = {'bindings/python/README.md', 'apps/web/assets/README.md'}
        for page in docs.source_pages(ROOT):
            relative = page.relative_to(ROOT).as_posix()
            self.assertTrue(relative in navigation or relative in supplementary,
                            f'{relative}: add a canonical navigation entry')


if __name__ == '__main__':
    unittest.main()
