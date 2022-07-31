# Security Policy

## Supported Versions

At the moment, only the latest commit on the `main` branch will be supported for security vulnerabilities. Private server operators
should keep their instances up to date and forks should regularily rebase on `main`.

| Branch  | Supported          |
| ------- | ------------------ |
| `main`  | :white_check_mark: |

## Reporting a Vulnerability

If you found a security vulnerability in DLU, please send a message to [darkflame-security@googlegroups.com][darkflame-security]. You should get a
reply within *72 hours* that we have received your report and a tentative [CVSS score](https://nvd.nist.gov/vuln-metrics/cvss/v3-calculator).
We will do a preliminary analysis to confirm that the vulnerability is a plausible claim and decline the report otherwise.

If possible, please include

1. reproducible steps on how to trigger the vulnerability
2. a description on why you are convinced that it exists.
3. any information you may have on active exploitation of the vulnerability (zero-day).

## Security Advisories

The project will release advisories on resolved vulnerabilities at <https://github.com/DarkflameUniverse/DarkflameServer/security/advisories>

## Receiving Security Updates

We set up [darkflame-security-announce@googlegroups.com][darkflame-security-announce] for private server operators to receive updates on vulnerabilities
such as the release of [Security Advisories](#security-advisories) or early workarounds and recommendations to mitigate ongoing
vulnerabilities.

Unfortunately, we cannot guarantee that announcements will be sent for every vulnerability.

## Embargo

We propose a 90 day (approx. 3 months) embargo on security vulnerabilities. That is, we ask everyone not to disclose the vulnerabilty
publicly until either:

1. 90 days have passed from the time the first related email is sent to `darkflame-security@`
2. a security advisory related to the vulnerability has been published by the project.

If you fail to comply with this embargo, you might be exluded from [receiving security updates](#receiving-security-updates).

## Bug Bounty

Unfortunately we cannot provide bug bounties at this time.

[darkflame-security]: mailto:darkflame-security@googlegroups.com
[darkflame-security-announce]: https://groups.google.com/g/darkflame-security-announce
