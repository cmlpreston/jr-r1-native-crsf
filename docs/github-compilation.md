# GitHub firmware compilation

`.github/workflows/firmware.yml` builds on standard Ubuntu GitHub-hosted runners
when source changes are pushed to `main` or `codex/**`. Documentation-only
changes do not trigger builds. It can also be started manually from Actions.
There is one job and one compiler process at a time, a 30-minute timeout, and
new pushes cancel obsolete runs for the same branch. No secrets are required.

The job uses Python 3.11, PlatformIO Core 6.1.19 and the dependencies pinned in
`platformio.ini`. It validates the installed hardware configuration, builds only
`seeed_xiao_esp32s3-pcb` (XIAO ESP32-S3, Waveshare LCD(F), 480x320 landscape),
then runs `make -C test -j1 run`, including the C++, Python and JavaScript tests.
The simulator and UI audit are separate checks, used when a change warrants
them; this workflow does not build or audit the simulator.

Open [the repository Actions page](https://github.com/cmlpreston/jr-r1-native-crsf/actions),
select **R1 firmware and tests**, then open a run to follow its live output.
For a manual build, choose **Run workflow** and the desired branch. Download
the `r1-landscape-<commit>` artifact after success; it contains the upload
package, exact committed source archive and manifest. A separate logs artifact
is saved even when a build/test step fails. Both expire after three days.

These packages use the existing artifact-preflight format, including hashes for
the application, bootloader, partition table, source archive and framework
`boot_app0`. The manifest explicitly records that no UI audit ran. Downloaded
firmware still requires the normal AGENTS.md review, fresh `origin/main`
ancestry check, matching hardware, relevant tests and explicit user-authorized
PlatformIO upload. No CI step opens a USB device or uploads firmware.

PlatformIO tool downloads are cached; firmware objects and test binaries are
rebuilt. Cache keys include the OS, CPU architecture, Core pin and configuration.
No private receiver archives, Codex settings, handset settings or credentials
are uploaded. Keep private files in the ignored local directories.

## Watch and download

Open the repository's **Actions → R1 firmware and tests**. Select a run and the
`landscape` job to follow live build/test output. After success, download
`r1-landscape-<full commit SHA>` from the run's Artifacts section. Logs are
available in `r1-logs-<full commit SHA>-<attempt>`, including on failed checks.
Artifacts expire after three days; download any firmware you want to retain.

With an authenticated GitHub CLI:

```sh
gh run list --repo cmlpreston/jr-r1-native-crsf --workflow firmware.yml
gh run watch RUN_ID --repo cmlpreston/jr-r1-native-crsf --exit-status
gh run view RUN_ID --repo cmlpreston/jr-r1-native-crsf --log
gh run download RUN_ID --repo cmlpreston/jr-r1-native-crsf \
  --name r1-landscape-FULL_COMMIT_SHA --dir /tmp/r1-ci-download
```

The firmware artifact contains `r1-upload.tar.gz`, a checksum/identity manifest
and `source.tar.gz` of the exact committed source. The upload package layout is
compatible with `tools/artifact_preflight.py`; its attestation lists firmware
and host checks explicitly and records that the UI audit was not run.
Neither downloading nor building authorizes flashing. Before any local upload,
follow `AGENTS.md`, confirm current main ancestry and the connected hardware,
review the exact tested commit and any needed UI checks, then use PlatformIO's
guarded upload target. GitHub never connects to the transmitter.

## Keep usage free

Standard hosted runners are free for public repositories. Private repositories
consume the owner's included allowance, shared with other eligible usage.
Short retention, caching, cancellation and skipping simulator builds reduce
usage; YAML cannot enforce an account-wide zero-spend policy.

Before enabling paid overage, inspect GitHub **Settings → Billing and licensing
→ Budgets and alerts**. Keep Actions usage blocked when the included allowance
is exhausted; if a budget is used, enable **Stop usage when budget limit is
reached**. Check storage as well as compute usage. Do not enable larger paid
runners or change repository visibility merely to get free builds.

See [GitHub Actions billing](https://docs.github.com/en/billing/concepts/product-billing/github-actions)
and [included plan usage](https://docs.github.com/en/billing/reference/product-usage-included)
for current allowances. Billing controls must be verified on the owner's account.

Local setup verification: Actionlint v1.7.12 accepts the workflow, the complete
single-job host suite passes on the Pi, and packaging tests verify compatibility
with artifact preflight, committed source identity and tamper detection. Local
setup log: `/tmp/r1-github-setup-tests.log`. The first actual GitHub run is a
separate validation; these local checks do not establish a passing hosted run.
