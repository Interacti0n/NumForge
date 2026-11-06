import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('git_hygiene', ROOT / 'scripts/check_git_hygiene.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class HygieneChecks(unittest.TestCase):
    def test_private_generated_and_real_environment_paths_are_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            root = Path(temporary)
            for name in ('.local/password', 'nested/node_modules/pkg/index.js', '.env.local', 'build-debug/output', 'output.exe'):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('fixture')
                self.assertTrue(any('Private/generated' in e for e in checker.audit(root, [name])), name)

    def test_source_templates_assets_and_test_passwords_are_allowed(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            root = Path(temporary)
            names = ['.env.example', 'src/math.c', 'tests/fixture.py', 'assets/logo.png']
            for name in names:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('password="not-a-real-account-password"')
            self.assertEqual(checker.audit(root, names), [])

    def test_secret_detection_reports_only_path(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            root = Path(temporary)
            token = 'ghp_' + 'A' * 36
            (root / 'config.txt').write_text(token)
            errors = checker.audit(root, ['config.txt'])
            self.assertEqual(len(errors), 1)
            self.assertIn('config.txt', errors[0])
            self.assertNotIn(token, errors[0])

    def test_untracked_build_inputs_fail_even_if_present_on_disk(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            root = Path(temporary)
            (root / 'deploy').mkdir()
            (root / 'deploy/app.Dockerfile').write_text('FROM base\nCOPY deploy/needed.py /app/\nCOPY --from=builder /out /app/\n')
            (root / 'deploy/needed.py').touch()
            errors = checker.audit(root, ['deploy/app.Dockerfile'])
            self.assertEqual(len(errors), 1)
            self.assertIn('COPY input is not tracked', errors[0])
            self.assertEqual(checker.audit(root, ['deploy/app.Dockerfile', 'deploy/needed.py']), [])

    def test_required_web_assets_must_be_in_git(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            root = Path(temporary)
            (root / 'apps/web/assets').mkdir(parents=True)
            cmake = 'apps/web/assets/CMakeLists.txt'
            asset = 'apps/web/assets/workspaces.js'
            (root / cmake).write_text('set(ASSETS\n    ' + asset + '\n)\n')
            (root / asset).touch()
            self.assertEqual(checker.audit(root, [cmake]), ['Web build asset is not tracked: ' + asset])
            self.assertEqual(checker.audit(root, [cmake, asset]), [])


if __name__ == '__main__':
    (ROOT / 'build').mkdir(exist_ok=True)
    unittest.main()
