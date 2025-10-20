// release/release.config.js
const isGitHub = !!process.env.GITHUB_ACTIONS;
export default {
  branches: ['main', { name: 'develop', channel: 'develop', prerelease: 'dev' }, { name: 'testing', channel: 'testing', prerelease: 'rc' }],
  repositoryUrl: isGitHub && process.env.GITHUB_REPOSITORY
      ? `https://github.com/${process.env.GITHUB_REPOSITORY}`
      : process.env.CI_PROJECT_URL,
  ci: true,
  plugins: [
    '@semantic-release/commit-analyzer',
    '@semantic-release/release-notes-generator',
    ...(isGitHub
        ? [
          ['@semantic-release/changelog', { changelogFile: 'CHANGELOG.md' }],
          ['@semantic-release/git', { assets: ['CHANGELOG.md'], message: 'chore(release): ${nextRelease.version} [skip ci]\n\n${nextRelease.notes}' }],
          '@semantic-release/github'
        ]
        : [
          ['@semantic-release/changelog', { changelogFile: 'CHANGELOG.md' }],
          ['@semantic-release/git', { assets: ['CHANGELOG.md'], message: 'chore(release): ${nextRelease.version} [skip ci]\n\n${nextRelease.notes}' }],
          ['@semantic-release/gitlab', { gitlabUrl: process.env.CI_SERVER_URL, gitlabApiPathPrefix: '/api/v4' }]
        ])
  ]
}