import copy
import unittest

from check_workspace_rollout import migration_preserved


class MigrationChecks(unittest.TestCase):
    def fixture(self):
        before = {'tables': {'identity_user': {'rows': 2, 'sha256': 'old'}, 'django_migrations': {'rows': 3, 'sha256': 'ledger'},
                             'django_content_type': {'rows': 1, 'sha256': 'types'}, 'auth_permission': {'rows': 1, 'sha256': 'permissions'}},
                  'roles': [['nf_accounts', False]], 'permissions': {'identity_user': 'original grants'},
                  'migrations': [['identity', '0003_preferences']],
                  'metadata': {'types': [[1, 'identity', 'user']], 'permissions': [[1, 'Can view user', 1, 'view_user']]}}
        after = copy.deepcopy(before)
        after['tables']['identity_workspace'] = {'rows': 0, 'sha256': 'empty'}
        after['tables']['django_migrations'] = {'rows': 4, 'sha256': 'new ledger'}
        after['migrations'].append(['identity', '0004_workspace'])
        after['metadata']['types'].append([2, 'identity', 'workspace'])
        after['metadata']['permissions'] += [[i, 'Can ' + action + ' workspace', 2, action + '_workspace']
                                           for i, action in enumerate(('add', 'change', 'delete', 'view'), 2)]
        after['tables']['django_content_type'] = {'rows': 2, 'sha256': 'new types'}
        after['tables']['auth_permission'] = {'rows': 5, 'sha256': 'new permissions'}
        return before, after

    def test_additive_workspace_table_and_exact_migration_are_accepted(self):
        before, after = self.fixture()
        migration_preserved(before, after)

    def test_original_row_grant_or_role_changes_are_rejected(self):
        for changed in ('rows', 'grants', 'roles'):
            with self.subTest(changed=changed):
                before, after = self.fixture()
                if changed == 'rows':
                    after['tables']['identity_user']['sha256'] = 'changed'
                elif changed == 'grants':
                    after['permissions']['identity_user'] = 'different grants'
                else:
                    after['roles'][0][1] = True
                with self.assertRaises(ValueError):
                    migration_preserved(before, after)

    def test_unexpected_table_and_ledger_changes_are_rejected(self):
        for changed in ('tables', 'ledger'):
            with self.subTest(changed=changed):
                before, after = self.fixture()
                if changed == 'tables':
                    after['tables']['unexpected'] = {'rows': 0}
                else:
                    after['migrations'].append(['identity', 'unexpected'])
                with self.assertRaises(ValueError):
                    migration_preserved(before, after)

    def test_old_metadata_changes_and_extra_permissions_are_rejected(self):
        for changed in ('old permission', 'new permission', 'new type'):
            with self.subTest(changed=changed):
                before, after = self.fixture()
                if changed == 'old permission':
                    after['metadata']['permissions'][0][1] = 'changed'
                elif changed == 'new permission':
                    after['metadata']['permissions'][-1][-1] = 'unexpected'
                else:
                    after['metadata']['types'][-1][1] = 'other_app'
                with self.assertRaises(ValueError):
                    migration_preserved(before, after)


if __name__ == '__main__':
    unittest.main()
