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
the `r1-landscape-<commit>` artifact after success; it contains the small upload
package and manifest. The exact committed source archive is a separate
`r1-source-<commit>` artifact. A separate logs artifact
is saved even when a build/test step fails. Both expire after three days.

These packages use the existing artifact-preflight format, including hashes for
the application, bootloader, partition table, source archive and framework
`boot_app0`. The manifest explicitly records that no UI audit ran. Downloaded
firmware still requires the normal AGENTS.md review, fresh `origin/main`
ancestry check, matching hardware, relevant tests and explicit user-authorized
PlatformIO upload. No CI step opens a USB device or uploads firmware.

PlatformIO tool downloads and ccache compiler results are cached. The ccache
directory is capped at 1 GiB; cache keys include OS, architecture and build
configuration, with a unique save key for each run. Each fresh runner restores
the latest compatible cache. Compiler binaries are checked by content and
ordinary header/flag checks remain enabled, with no relaxed correctness options.
Builds still check every source; a printed `Compiling` line can be a cache hit.
The job summary and `ccache.log` report actual hit/miss counts.

The CI-only PRE script wraps C/C++ compilation, including framework/library
sources. Native compiler invocations also pass through ccache, but most native
targets compile and link in one command and remain uncacheable. Linking and
assembly run normally; the host suite still takes about 22 seconds.
The extra-script environment setting appends to `tools/upload_preflight.py`,
preserving its source identity; CI checks that both scripts occur once.
Local PlatformIO configuration and upload guards remain
unchanged. The first ccache run populates the cache; subsequent runs can reuse
unchanged results even after a fresh checkout. Changed commit identity still
recompiles code that embeds that identity. Cache entries can be evicted under
GitHub's repository storage limit; a missing cache causes a normal compilation.
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

The firmware artifact contains only `r1-upload.tar.gz` and a checksum/identity
manifest. Routine flashing downloads this artifact alone; do not download all
run artifacts by omitting `--name`. The separate `r1-source-<full commit SHA>`
artifact contains `source.tar.gz` and the same manifest, for optional source
inspection or archival. Its checksum remains recorded in the firmware manifest;
the source archive is not required by the Pi artifact-upload verifier. The
firmware, bootloader, partition and configuration checks remain unchanged.
The upload package layout is
compatible with `tools/artifact_preflight.py`; its attestation lists firmware
and host checks explicitly and records that the UI audit was not run.
Neither downloading nor building authorizes flashing. Before any local upload,
follow `AGENTS.md`, confirm current main ancestry and the connected hardware,
review the exact tested commit and any needed UI checks, then use PlatformIO's
guarded upload target. GitHub never connects to the transmitter.

Both packages are already gzip-compressed, so artifact uploads use compression
level zero to avoid recompressing them. Firmware, source and logs retain the
same three-day expiry.

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

## Hosted validation — 17 November 2026

GitHub sign-in and workflow publication are complete. The repository was
confirmed public, with Actions enabled, so standard hosted-runner compute is
free independently of the private-repository allowance. The dedicated read-only
SSH key was preserved; publication used the separately authorized HTTPS login.
No paid runner, increased cache allowance or billing setting was enabled.

[The first hosted run](https://github.com/cmlpreston/jr-r1-native-crsf/actions/runs/44060405986)
passes at source commit `de13f6b707fcbcfb33f731eef06a03d682e121c2` on
`codex/r1-v2`. The complete job took 3m48s, including a cold toolchain download,
single-job firmware build (2m11s), complete host suite and artifact upload.
Firmware RAM is 204284 bytes; flash is 2241793 bytes. The first cache occupies
about 963 MiB, below the default 10 GiB allowance; firmware/source artifacts
total about 53 MiB and expire after three days.

Downloaded artifacts also pass the existing Pi artifact verifier: source archive
hash, image/configuration hashes, hardware profile/pins and the local framework's
`boot_app0` match. Firmware SHA256:
`2e832ad2349bf26697d6137992688401d85f186947326dd85df02e0a4780725b`.
Download and verification directory: `/tmp/r1-github-build-de13f6b`;
hosted log: `/tmp/r1-github-build-de13f6b.log`. No hardware was contacted.

The original workflow emitted a nonfatal Node 20 action-runtime deprecation
warning. The workflow now uses explicit Node 24 action runtimes: checkout v6,
setup-python v6, setup-node v6, cache v5 and upload-artifact v6. These versions
support the existing inputs and zipped firmware/source downloads. The configured
Node 22 used for project JavaScript tests is separate from the actions' runtime.
Automatic package-manager caching is disabled because these tests run directly
with Node; the explicit PlatformIO/ccache caches retain their existing keys.

Local setup verification: Actionlint v1.7.12 accepts the workflow, the complete
single-job host suite passes on the Pi, and packaging tests verify compatibility
with artifact preflight, committed source identity and tamper detection. Local
setup log: `/tmp/r1-github-setup-tests.log`. The successful hosted runs below
also validate the actual GitHub environment.

## Compiler cache measurements — 17 November

The [cache-populating run](https://github.com/cmlpreston/jr-r1-native-crsf/actions/runs/44063243172)
passed with 395 cacheable misses. The subsequent
[comment-only rebuild](https://github.com/cmlpreston/jr-r1-native-crsf/actions/runs/44064002274)
passed with 393/395 cache hits (99.49%; 390 preprocessed hits, three direct).
Embedded Git identity remains current; the downloaded image contains `67bd5ab3`.

| Stage | Populate cache | Reuse cache |
| --- | ---: | ---: |
| Firmware step | 118 s | 49 s |
| Complete host suite | 23 s | 22 s |
| Entire job, excluding queue | 197 s | 117 s |

The measured job improvement is about 41%. PlatformIO still prints `Compiling`
when ccache returns an object. Changing the global Git identity definitions
requires preprocessing to check most cache entries; most hits are therefore
preprocessed hits rather than direct hits.

## Default R1 upload path

At the user's request, use GitHub for compilation and the complete host suite
on future authorized uploads. Avoid recompiling or rerunning that suite on the
Pi3. Keep Pi checks to artifact/source identity and hashes, fresh main ancestry,
approved hardware/profile/pins, guarded PlatformIO upload and read-only postboot
status. UI/simulator checks remain change-dependent.

Reuse `/home/pi/r1-dev/project/.pio/cloud-upload`, an ignored persistent
artifact workspace. Do not create a fresh project and reinstall libraries on
every upload. Download and validate the package before replacing workspace
inputs. For first-time setup, initialize PlatformIO before restoring packaged
binaries; its automatic initial cleanup can remove them. Subsequent verified
artifact uploads use `--disable-auto-clean -t nobuild -t upload`, the approved
environment, explicit matching USB port and `JR_R1_ARTIFACT_MANIFEST` so the
existing artifact guard checks all image/configuration hashes before writing.
Disabling automatic build cleanup does not disable the upload preflight.

The user-authorized upload of `67bd5ab36984915cb897c966b1184943926464ba`
completed on 17 November, using the approved XIAO/Waveshare landscape profile.
Firmware SHA256:
`d09d51aaefaa8c90c47e220f1c229ce539a73880453a83814922cf4396a763a8`.
All flash hashes verified. Read-only status at uptime 10.3 s reports CUT, both
MCPs online with zero read errors, outputs 992/992, no RF bench authorization
and no arm request. Opening USB serial may restart the XIAO.

| Stage | Measured duration |
| --- | ---: |
| Cached GitHub build/tests/package job | 117 s |
| Download existing combined firmware/source artifact | 42.87 s |
| Source/image hash, embedded identity and hardware verification | 1.52 s |
| Guarded upload using prepared workspace | 49.24 s |
| Application transfer within upload | 26.1 s |

The stage sum through flash completion is 210.6 s (about 3m31s); allow roughly
3m40s including startup. This is a stage-based estimate, excluding queue time,
conversation pauses, fresh-main fetch overhead and the one-time workspace setup.
The first attempt spent 60.98 s preparing dependencies, then correctly refused
because initial cleanup removed the staged images; no hardware was written.
Preparation/restoration added another 16.54 s before the successful retry.
Logs, downloaded package and timing JSON are in `/tmp/r1-timed-cloud-flash`.

Firmware and source uploads are now separate artifacts, so routine flashing
downloads only the small firmware package. The table above records the earlier
combined-artifact baseline.

The [split-artifact validation run](https://github.com/cmlpreston/jr-r1-native-crsf/actions/runs/44066365424)
passed the firmware build and complete host suite at `fc46579b`. Uploaded sizes
are 1,210,422 bytes for firmware and 54,416,821 bytes for the separate source
archive. A timed firmware-only download on the Pi took 4.25 s, compared with
42.87 s for the earlier combined artifact (about 90% less download time).
The existing artifact preflight and embedded commit-identity check passed in
0.29 s without downloading the source archive. Measurement and extracted
verification files are in `/tmp/r1-split-firmware-download`. No upload was
performed for this packaging-only change; the persistent upload workspace was
not replaced. Combining the new download timing with the earlier build/upload
measurements suggests roughly three minutes including startup, excluding queue
time and fresh-main fetch overhead; this is an estimate rather than another
measured complete upload.

Next opportunities: reduce runner setup overhead; separate native
compilation/linking to cache the host
objects; scope identity definitions to the sources that consume them to improve
direct cache hits. Keep correct embedded identity, hash checks and upload guards.
