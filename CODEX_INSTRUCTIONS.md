# CODEX Instructions

You work in my local git repository and must follow this standard release and GitHub workflow for all new public versions.

## General release standard

1. Always include the version number in all public release asset filenames.
2. Use this naming scheme consistently, with no spaces:

   Name-version-platform-architecture

3. Keep filenames ASCII-friendly and scriptable.
4. Do not mix naming styles across platforms.

## Example asset names

- DJwerk-v0.2.3-Windows-x64.exe
- DJwerk-v0.2.3-Linux-x86_64.AppImage
- DJwerk-v0.2.3-macOS-Silicon-arm64.zip
- DJwerk-v0.2.3-macOS-Intel-x86_64.zip

## Filename rules

- No spaces
- Use hyphens as separators
- Keep platform and architecture labels consistent
- The version in the filename must exactly match the git tag / release tag
- Do not use generic names like:
  - DJwerk.exe
  - latest.zip
  - DJwerk-mac.zip
- Do not upload unpacked .app directories or build folders as release assets

## What to upload to GitHub Releases

Upload only final end-user distributables, such as:

- Windows: .exe or user-facing .zip
- Linux: .AppImage or other end-user artifact
- macOS: user-facing .zip or .dmg

## What not to upload

- Build directories
- .app directories as folders
- Logs
- Cache
- Temporary artifacts
- Debug symbols, unless explicitly intended for distribution
- Duplicate assets with old naming conventions

## GitHub / gh workflow

For every new public version:

1. Check git status
2. Ensure all intended changes are committed
3. Push the branch
4. Create a new version tag
5. Push the tag
6. Create the GitHub release
7. Upload only final release assets with version numbers in the filenames
8. Verify the published release contains the correct assets

## Preferred CLI workflow

Use gh CLI for releases when possible.

Typical sequence:

`ash
git push origin <branch>
git tag -a <version> <commit> -m "<project> <version>"
git push origin <version>
gh release create <version> <asset1> <asset2> ... --title "<project> <version>" --notes "<release notes>"
`

## If a release already exists

- Do not silently move an existing public release tag
- Prefer a new version, for example 0.2.4 instead of replacing 0.2.3
- Only replace assets in an existing release if I explicitly ask for it
- Be careful with immutable releases or locked tags

## Safety and consistency rules

- A GitHub release must match the code on the corresponding tag
- Do not upload assets without version numbers in their filenames
- Do not publish a release if assets still use old naming
- If naming is inconsistent, rename locally before upload
- If elease_assets/ exists, inspect it first and upload only public distribution files

## Pre-publish checklist

Always check:

1. Does the version in every asset filename match the tag?
2. Are platform and architecture labels correct?
3. Are there no folders or temporary build artifacts selected?
4. Is the release tied to the correct commit or tag?
5. Are release notes present?
6. Is the asset set complete for the intended platforms?

## Post-publish checklist

After publishing:

1. View the release
2. Confirm the correct title and tag are visible
3. Confirm all assets include the correct version in the filename
4. Confirm there are no old or non-versioned assets mixed in
5. Report which release was published, which assets were included, and anything intentionally excluded

## If you are unsure

- Do not ask unnecessary questions when the intent is clear
- Inspect elease_assets/ or build output first
- Propose exact filenames and exact gh commands
- Be strict about consistency