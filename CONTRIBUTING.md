# Contributing

Thanks for helping improve DIODAC / iSystem open source music projects.

## Workflow

1. Fork the repository.
2. Create a focused branch for your change.
3. Keep changes scoped to one project or clearly related set of files.
4. Open a pull request with a short description, tested hardware/software, and any known limitations.

## License

By contributing, you agree that your contribution is provided under the repository's MIT License.

## Secrets and Private Data

Do not commit passwords, WiFi credentials, API keys, private certificates, personal data, or generated local configuration files. Use placeholders in examples and document any provisioning flow instead.

## Code Style

- Preserve the existing style and formatting of each project.
- Keep firmware logic changes small and easy to review.
- Do not mass-rename hardware manufacturing files unless a project maintainer requests it.
- Document required libraries, board targets, and manual test steps when adding or changing firmware.
- For JavaScript, use plain CommonJS/Manifest V3 patterns already present in the bridge unless a larger migration is discussed first.
