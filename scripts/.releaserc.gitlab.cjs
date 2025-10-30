export default {
    branches: ["main", "develop", "testing"],
    repositoryUrl: "https://gitlab.com/<group>/<repo>.git",
    plugins: [
        "@semantic-release/commit-analyzer",
        "@semantic-release/release-notes-generator",
        "@semantic-release/changelog",
        ["@semantic-release/git", {"assets": ["CHANGELOG.md"]}],
        "@semantic-release/gitlab"
    ]
};