OAuth 2.0
=========

nmail supports OAuth 2.0 authentication for Gmail and Outlook / Microsoft 365.
The setup wizard (`nmail -s`) offers OAuth 2.0 for these email services.

Getting Started
---------------

### Gmail Invite

nmail is not verified by Google, so Gmail OAuth 2.0 access is limited to
invited users. To request an invite, send an email to `d99kris at gmail dot com`
with subject `nmail google oauth2 invite` from the Google account to invite.

Alternatively you may set up your own OAuth 2.0 application, see
[Custom OAuth 2.0 Application](#custom-oauth-20-application).

### Gmail

Gmail requires IMAP access to be enabled, see
[Gmail Prerequisites](/README.md#gmail-prerequisites). Once invited, run
`nmail -s` and select OAuth2 as auth method.

### Outlook

Microsoft has disabled password authentication for most accounts, so the setup
wizard defaults to OAuth 2.0 for Outlook, Hotmail and Microsoft 365 accounts.
Microsoft 365 organizations may restrict IMAP access or the use of third-party
email apps, in which case an administrator needs to allow it.

### Requirements

OAuth 2.0 login is handled by a helper script bundled with nmail, which
requires `python3` (only its standard library is used).

### Login

During setup nmail opens a web browser with the email service login page.
After logging in and granting access, the browser is redirected to a temporary
local web server started by nmail, which receives the access tokens. The
browser window may then be closed. Access tokens are subsequently refreshed by
nmail automatically.

Customization
-------------

### Web Browser

The web browser used for login is controlled by the environment variable
`$BROWSER`, which can be set when running setup, for example:

    BROWSER=firefox nmail -s

Supported values are browser names known by the Python `webbrowser` module
(e.g. `firefox`, `chrome`, `safari`), or a command to which the login URL is
appended (e.g. `open -a Firefox`).

To persistently override `$BROWSER` for nmail, set `browser_cmd` in
`~/.config/nmail/main.conf`. This setting is read before the setup wizard, so
it applies when re-running setup, but not on first setup when no config file
exists yet.

### Token Encryption

Access tokens are stored in `~/.config/nmail/cache/auth/`. By default they are
not encrypted. To encrypt them, along with the message cache, run setup with
`-c`:

    nmail -s -c

The setup wizard then prompts for a cache encryption password. Unless saved,
it is prompted for each time nmail starts. Related config parameters in
`main.conf` are `auth_encrypt` and `cache_encrypt`.

### Custom OAuth 2.0 Application

By default nmail uses its own OAuth 2.0 application. Users may register their
own application with the email service, and configure its client id and secret
in `~/.config/nmail/auth.conf`:

    oauth2_client_id=
    oauth2_client_secret=

Detailed instructions for registering an application are not yet available.
