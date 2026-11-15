Providers
=========

Email provider settings used by the nmail setup wizard (`nmail -s`). Each
`<id>.conf` file is embedded in the nmail executable at build time, and `<id>`
can be passed to the wizard, e.g. `nmail -s gmail`.

The wizard looks up a provider by email domain (`domains`), and if there is no
match, by the domain's MX records (`mx`). If neither matches, the user is
prompted for server settings.

Special folders (sent, drafts, trash, spam) are normally detected over IMAP at
first login, so they only need to be specified for servers that do not report
them.

Keys
----
Provider keys:

    name        display name
    domains     comma-separated email domains
    mx          comma-separated MX host suffixes, e.g. google.com matches
                aspmx.l.google.com, longest match wins across providers
    auth        comma-separated auth methods, first is default, e.g.
                pass,gmail-oauth2 (default: pass)
    pass_hint   short note shown for password in auth method selection, e.g.
                requires app password
    pass_note   shown before password prompt, e.g. where to create an app
                password
    oauth_hint  short note shown for oauth2 in auth method selection, e.g.
                requires invite
    oauth_note  shown before oauth2 login, e.g. how to obtain access
    user_format username format, email (default) or localpart for username
                without domain

All other keys are copied to `main.conf`, e.g. `imap_host`, `imap_port`,
`smtp_host`, `smtp_port`, `sent`, `folders_exclude`, `client_store_sent`.

Guidelines
----------
Public email services are welcome, also those using standard server names, as
a provider spares users from entering server details. Listing primary domains
is usually enough, as other domains using the same mail servers are detected by
MX lookup (see `mx`). This includes vanity domains, and company or personal
domains hosted by e.g. Google Workspace or Microsoft 365, which are then
covered by `gmail.conf` and `outlook.conf`.
