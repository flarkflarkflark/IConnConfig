# Claude Code Instructions

## Git authorship and commit policy

Never add Claude, Anthropic, Claude Sonnet, or any AI assistant as a commit author, committer, co-author, signer, or trailer.

Do not add lines like:

`Co-Authored-By: Claude Sonnet <noreply@anthropic.com>`
`Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>`
`Co-Authored-By: Claude <noreply@anthropic.com>`
`Generated-by: Claude`
`AI-generated-by: Claude`

All commits must use the user's configured Git identity only.

Before creating a commit, always inspect the proposed commit message and ensure it contains no AI co-author, AI attribution, or Anthropic metadata.

When asked to commit, use normal commit messages only, for example:

`fix: improve setup repair flow`

Never append co-author trailers unless the user explicitly provides a human co-author line.

## History rewrite safety

Never rewrite public history, force-push, move tags, delete tags, or modify release tags without explicit confirmation from the user.

If a commit is reachable from a release tag, stop and report the risk before proceeding.

## Release safety

For public release assets, always include the version number in filenames.

Use consistent hyphenated names with no spaces.

Preferred format:

`ProjectName-vX.Y.Z-platform-architecture.ext`

Examples:

`STEMwerk-v2.2.2.1-Windows-x64.exe`
`THEMEwerk-v0.1.8-ReaPack.zip`
`DJwerk-v0.2.3-Linux-x64.AppImage`
