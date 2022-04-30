nmail - ncurses mail
====================

| **Linux** | **Mac** |
|-----------|---------|
| [![Linux](https://github.com/d99kris/nmail/workflows/Linux/badge.svg)](https://github.com/d99kris/nmail/actions?query=workflow%3ALinux) | [![macOS](https://github.com/d99kris/nmail/workflows/macOS/badge.svg)](https://github.com/d99kris/nmail/actions?query=workflow%3AmacOS) |

nmail is a terminal-based email client for Linux and macOS with a user interface
similar to alpine / pine.

![screenshot nmail](/doc/screenshot-nmail.png)

Features
--------
- Support for IMAP and SMTP protocols
- Local cache using sqlite (optionally AES256-encrypted)
- Multi-threaded (email fetch and send done in background)
- Address book auto-generated based on email messages
- Viewing HTML emails (converted to text in terminal, or in external browser)
- Opening/viewing attachments in external program
- Simple setup wizard for Gmail and Outlook/Hotmail
- Familiar UI for alpine / pine users
- Compose message using external editor ($EDITOR)
- View message using external viewer ($PAGER)
- Saving and continuing draft messages
- Compose HTML emails using Markdown (see `markdown_html_compose` option)
- Email search
- Compose emails while offline
- Color customization
- Signature

Not Supported / Out of Scope
----------------------------
- Local mailbox downloaded by third-party application (OfflineIMAP, fdm, etc)
- Multiple email accounts in a single session
- Special handling for Gmail labels
- Threaded view


Usage
=====

Usage:

    nmail [OPTION]

Command-line Options:

    -d, --confdir <DIR>
        use a different directory than ~/.nmail

    -e, --verbose
        enable verbose logging

    -ee, --extra-verbose
        enable extra verbose logging

    -h, --help
        display this help and exit

    -o, --offline
        run in offline mode

    -p, --pass
        change password

    -s, --setup <SERV>
        setup wizard for specified service, supported services: gmail,
        gmail-oauth2, outlook

    -v, --version
        output version information and exit

    -x, --export <DIR>
        export cache to specified dir in Maildir format

Configuration files:

    ~/.nmail/auth.conf
        configures custom oauth2 client id and secret

    ~/.nmail/main.conf
        configures mail account and general setings

    ~/.nmail/ui.conf
        customizes UI settings

    ~/.nmail/secret.conf
        stores saved passwords

Examples:

    nmail -s gmail
        setup nmail for a gmail account


Supported Platforms
===================

nmail is developed and tested on Linux and macOS. Current version has been
tested on:

- macOS Big Sur 11.0
- Ubuntu 20.04 LTS


Build / Install
===============

Linux
-----

**Dependencies Ubuntu**

Required:

    sudo apt install git cmake build-essential libetpan-dev libssl-dev libncurses-dev libxapian-dev libsqlite3-dev libsasl2-modules libmagic-dev uuid-dev

Optional (to view/compose HTML emails):

    sudo apt install pandoc

**Dependencies Fedora**

Required:

    sudo yum -y install cmake libetpan-devel openssl-devel ncurses-devel xapian-core-devel sqlite-devel cyrus-sasl-devel cyrus-sasl-plain file-devel libuuid-devel clang

Optional (to view/compose HTML emails):

    sudo yum -y install pandoc

**Dependencies Arch Linux**

Required:

    sudo pacman -Sy cmake make libetpan openssl ncurses xapian-core sqlite cyrus-sasl file uuid clang

Optional (to view/compose HTML emails):

    sudo pacman -Sy pandoc

**Dependencies Common Linux**

Optional (for OAuth 2.0 support):

    pip3 install -U requests

**Source**

    git clone https://github.com/d99kris/nmail && cd nmail

**Build**

    mkdir -p build && cd build && cmake .. && make -s

**Install**

    sudo make install

macOS
-----

**Dependencies**

Required:

    brew install cmake libetpan openssl ncurses xapian sqlite libmagic ossp-uuid

Optional (for OAuth 2.0 support):

    pip3 install -U requests

Optional (to view/compose HTML emails):

    brew install pandoc

**Source**

    git clone https://github.com/d99kris/nmail && cd nmail

**Build**

    mkdir -p build && cd build && cmake .. && make -s

**Install**

    make install


Getting Started
===============

Gmail Password Authenticated Setup
----------------------------------
Use the setup wizard to set up nmail for the account. Example (replace
example@gmail.com with your actual gmail address):

    $ nmail -s gmail
    Email: example@gmail.com
    Name: Firstname Lastname
    Password:
    Save password (y/n): y

Note: Refer to [Gmail Prerequisites](#gmail-prerequisites) for enabling
IMAP access and password authentication.

Gmail OAuth 2.0 Setup
---------------------
Use the setup wizard to set up nmail for the account. Example:

    $ nmail -s gmail-oauth2
    Cache Encryption Password (optional):
    Save password (y/n): y

Cache encryption password is optional, if not specified all local cache
encryption will be disabled.

Note: Refer to [Gmail Prerequisites](#gmail-prerequisites) for enabling
IMAP access and obtaining OAuth 2.0 access.

Outlook (and Hotmail) Setup
---------------------------
Use the setup wizard to set up nmail for the account. Example (replace
example@hotmail.com with your actual outlook / hotmail address):

    $ nmail -s outlook
    Email: example@hotmail.com
    Name: Firstname Lastname
    Password:
    Save password (y/n): y

Other Email Providers
---------------------
Run nmail once in order for it to automatically generate the default config
file:

    $ nmail

Then open the config file `~/.nmail/main.conf` in your favourite text editor
and fill out the required fields:

    address=example@example.com
    drafts=Drafts
    imap_host=imap.example.com
    imap_port=993
    inbox=INBOX
    name=Firstname Lastname
    smtp_host=smtp.example.com
    smtp_port=587
    trash=Trash
    user=example@example.com

Full example of a config file `~/.nmail/main.conf`:

    address=example@example.com
    addressbook_encrypt=0
    auth=pass
    auth_encrypt=1
    cache_encrypt=0
    cache_index_encrypt=0
    client_store_sent=0
    downloads_dir=
    drafts=Drafts
    editor_cmd=
    file_picker_cmd=
    folders_exclude=
    html_to_text_cmd=
    html_viewer_cmd=
    imap_host=imap.example.com
    imap_port=993
    inbox=INBOX
    msg_viewer_cmd=
    name=Firstname Lastname
    network_timeout=30
    pager_cmd=
    parts_viewer_cmd=
    prefetch_all_headers=1
    prefetch_level=2
    queue_encrypt=1
    save_pass=1
    sender_hostname=
    sent=Sent
    server_timestamps=0
    smtp_host=smtp.example.com
    smtp_port=587
    smtp_user=
    text_to_html_cmd=
    trash=Trash
    user=example@example.com
    verbose_logging=0

### address

The from-address to use. Required for sending emails.

### addressbook_encrypt

Indicates whether nmail shall encrypt local address book cache or not. Enabling
it has some performance impact when starting and exiting nmail (default
disabled).

### auth

Specifies whether nmail shall use password authentication (`pass`) or Gmail
OAuth 2.0 authentication (`gmail-oauth2`).

### auth_encrypt

Indicates whether nmail shall encrypt local OAuth 2.0 access token store
(default enabled).

### cache_encrypt

Indicates whether nmail shall encrypt local message cache or not. Enabling
it has some performance impact when starting and exiting nmail, as well as
when navigating to different folders (default disabled).

### cache_index_encrypt

Indicates whether nmail shall encrypt local search index or not. Enabling
it has some performance impact when starting and exiting nmail (default
disabled).

### client_store_sent

This field should generally be left `0`. It indicates whether nmail shall upload
sent emails to configured `sent` folder. Many email service providers
(gmail, outlook, etc) do this on server side, so this should only be enabled if
emails sent using nmail do not automatically gets stored in the sent folder.

### downloads_dir

Specifies a custom downloads directory path to save attachments to, for example
`downloads_dir=~/Downloads`. If not specified, the current working dir is used.

### drafts

Name of drafts folder - needed if using functionality to postpone email editing.

### editor_cmd

The field `editor_cmd` allows overriding which external editor to use when
composing / editing an email using an external editor (`Ctrl-E`). If not
specified, nmail will use the editor specified by the environment variable
`$EDITOR`. If `$EDITOR` is not set, nmail will use `nano`.

### file_picker_cmd

By default when using `Ctrl-T` to select attachment files, the nmail internal
file picker is used. By specifying this paramater an external command may be
used instead. The command must output selected file(s) separated by linebreaks,
on stdout. One may use `nnn` by setting the parameter like this:

```file_picker_cmd=TMP=$(mktemp); 2>&1 nnn -p ${TMP}; (cat ${TMP} | tr '\0' '\n' | uniq; rm ${TMP})```

### folders_exclude

This field allows excluding certain folders from being accessible in nmail
and also from being indexed by the search engine. This is mainly useful for
email service providers with "virtual" folders that are holding copies of
emails in other folders. When using the setup-wizard to configure a Gmail
account, this field will be configured to
`"[Gmail]/All Mail","[Gmail]/Important","[Gmail]/Starred"`. As an alternative
to configuring this parameter for Gmail, folders can be excluded from IMAP
access on server side. In Gmail web interface, navigate to "Settings",
"See all settings", "Labels" and untick "Show in IMAP" for "Starred",
"Important" and "All Mail".

### html_to_text_cmd

This field allows customizing how nmail should convert HTML emails to text.
If not specified, nmail checks if `pandoc`, `w3m`, `lynx` or `elinks` is
available on the system (in that order), and uses the first found. The exact
command used is one of:
- `pandoc -f html -t plain+literate_haskell --wrap=preserve`
- `w3m -T text/html -I utf-8 -dump`
- `lynx -assume_charset=utf-8 -display_charset=utf-8 -nomargins -dump -stdin`
- `elinks -dump-charset utf-8 -dump`

Note that while pandoc generally produces a better text-equivalent to an
html email, it is also slower than the other tools. For usage on a lower
spec'ed system, consider using any of the other conversion utilities instead.

### html_viewer_cmd

This field allows overriding the external viewer used when viewing message
as html and previewing messages composed using markdown. The viewer may be
a terminal-based program, e.g. `w3m -o confirm_qq=false`. By default nmail
uses `open` on macOS and `xdg-open >/dev/null 2>&1` on Linux.

### imap_host

IMAP hostname / address. Required for fetching emails.

### imap_port

IMAP port. Required for fetching emails.

### inbox

IMAP inbox folder name. Required for nmail to open the proper default folder.

### msg_viewer_cmd

This field allows overriding the command used for externally viewing a
message (`.eml` file) with `W`. By default nmail uses `open` on macOS and
`xdg-open >/dev/null 2>&1` on Linux.

### name

Real name of sender. Recommended when sending emails.

### network_timeout

Specify timeout for IMAP and SMTP operations in seconds. If using a very slow
network connection and sending very large emails it may be necessary to increase
this timeout. By setting it to 0 network operations will not time out.
Default 30 seconds.

### pager_cmd

The field `pager_cmd` allows overriding which external pager / text viewer to
use when viewing an email using an external pager (`E`). If not specified,
nmail will use the pager specified by the environment variable `$PAGER`.
If `$PAGER` is not set, nmail will use `less`.

### parts_viewer_cmd

This field allows overriding the external viewer used when viewing email
parts and attachments. By default nmail uses `open` on macOS and
`xdg-open >/dev/null 2>&1` on Linux.

### prefetch_all_headers

Determines whether nmail shall fetch headers for all messages when viewing a
folder, or only the latest based on message uid. By disabling this option there
is no guarantee folder message lists are sorted by timestamp, as only headers
for the last messages stored/added in the folder will be retrieved from server.
Also note that some other nmail features may operate in degraded mode when this
setting is disabled. The ability to disable prefetching of all headers is
mainly to encompass use-cases where one wants to minimize network usage, or
use nmail without persistant cache. Default enabled.

### prefetch_level

Messages are pre-fetched from server based on the `prefetch_level` config
setting. The following levels are supported:

    0 = no pre-fetching, messages are retrieved when viewed
    1 = pre-fetching of currently selected message
    2 = pre-fetching of all messages in current folder view (default)
    3 = pre-fetching of all messages in all folders, i.e. full sync

With level 0-2 configured, pre-fetch level 3 - a single full sync - may be
triggered at run-time by pressing `s` from the message list.

### queue_encrypt

Indicates whether nmail shall encrypt local message offline queue or not
(default enabled).

### save_pass

Specifies whether nmail shall store the password(s) (default enabled).

### sender_hostname

Custom sender hostname to use in SMTP authentication when sending outgoing
emails. If not set, nmail uses the system hostname (default).

### sent

IMAP sent folder name. Used by nmail if `client_store_sent` is enabled to store
copies of outgoing emails. 

### server_timestamps

Use server timestamps for messages, rather than the timestamp in the message
header (default disabled).

### smtp_host

SMTP hostname / address. Required for sending emails.

### smtp_port

SMTP port. Required for fetching emails. Default 587.

### smtp_user

The field `smtp_user` should generally be left blank, and only be specified in
case the email account has different username and password for sending emails
(or if one wants to use one email service provider for receiving and another
for sending emails). If not specified, the configured `user` field will be
used.

### text_to_html_cmd

This field allows customizing how nmail should convert composed plain text
markdown message to corresponding text/html part. If not specified, nmail
checks if `pandoc` or `markdown` is available on the system (in that order),
and uses the first found. The exact command used is one of:
- `pandoc -s -f gfm -t html`
- `markdown`

### trash

IMAP trash folder name. Needs to be specified in order to delete emails.

### user

Email account username for IMAP (and SMTP).

### verbose_logging

Allows forcing nmail to enable specified logging level:

    0 = info, warnings, errors (default)
    1 = debug (same as `-e`, `--verbose` - enable verbose logging)
    2 = trace (same as `-ee`, `--extra-verbose` - enable extra verbose logging)


Multiple Email Accounts
=======================

nmail does currently not support multiple email accounts (in a single session).
It is however possible to run multiple nmail instances in parallel with
different config directories (and thus different email accounts), but it will
be just that - multiple instances - each in its own terminal. To facilitate
such usage one can set up aliases for accessing different accounts, e.g.:

    alias gm='nmail -d ${HOME}/.nmail-gm' # gmail
    alias hm='nmail -d ${HOME}/.nmail-hm' # hotmail


Compose Editor
==============

The built-in email compose editor in nmail supports the following:

    Alt-Backspace  delete previous word
    Alt-Delete     delete next word
    Alt-Left       move the cursor backward one word
    Alt-Right      move the cursor forward one word
    Arrow keys     move the cursor
    Backspace      backspace
    Ctrl-C         cancel message
    Ctrl-E         edit message in external editor
    Ctrl-K         delete current line
    Ctrl-N         toggle markdown editing
    Ctrl-O         postpone message
    Ctrl-R         toggle rich headers (bcc)
    Ctrl-T         to select, from address book / from file dialog
    Ctrl-V         preview html part (using markdown to html conversion)
    Ctrl-X         send message
    Delete         delete
    Enter          new line
    Page Up/Down   move the cursor page up / down

The email headers `To`, `Cc` and `Attchmnt` support comma-separated values, ex:

    To      : Alice <alice@example.com>, Bob <bob@example.com>
    Cc      : Chuck <chuck@example.com>, Dave <dave@example.com>
    Attchmnt: localpath.txt, /tmp/absolutepath.txt
    Subject : Hello world

Attachment paths may be local (just filename) or absolute (full path).


Email Search
============

Press `/` in the message list view to search the local cache for an email. The
local cache can be fully syncronized with server by pressing `s`. The search
engine supports queries with `"quoted strings"`, `+musthave`, `-mustnothave`,
`partialstring*`, `AND`, `OR`, `XOR` and `NOT`.

Search terms may be prefixed by `body:`, `subject:`, `from:` or `to:` to
search only specified fields. By default search query terms are combined
with `AND` unless specified. Results are sorted by email timestamp.

Press `<` or `Left` to exit search results and go back to current folder
message list.


Reporting Bugs
==============

The preferred way of reporting bugs is by opening
[a Github issue](https://github.com/d99kris/nmail/issues/new). Providing a
copy of `~/.nmail/log.txt` when reporting the issue is preferred.

Different verbosity levels are supported. The default logging level is very
brief, primarily to minimize performance overhead. However, when encountering
issued and reporting bugs, it is generally preferred to provide logs produced
in verbose mode. To enable verbose logging:

    nmail --verbose

For certain low-level bugs it may be required to collect extra verbose logs.
This produces very large log files with detailed information. The extra verbose
logs typically contain actual email contents. Review and edit such logs to
remove any private information before sharing. To enable extra verbose logging:

    nmail --extra-verbose

Default logging verbosity can be configured with `verbose_logging` parameter in
`~/.nmail/main.conf`.

Finally, for issues where the logging above does not provide sufficient
information, it may be necessary to run nmail through a debugger and capture
detailed backtraces for all threads when an issue occurs. A simple utility
is provided to run `nmail` through a debugger in the source package. Example
usage:

    ./utils/debug-nmail.sh

The utility also supports passing arguments to nmail, enabling running nmail
with a custom config directory, for example:

    ./utils/debug-nmail.sh -d ~/.nmail-hm

Once nmail exits, either normally or caused by a crash, the script outputs
information on where to obtain the log file, for example:

    gdb log file written to:
    /tmp/nmail-gdb-5244.txt

Review the resulting log file and ensure there are no clear text passwords
in it, before sharing it with others.


User Discussion Forums
======================

For discussing nmail usage (including getting help from other users) a mailing
list and a Telegram group are available.

Bug reports and feature requests should however be reported using
[Github issues](https://github.com/d99kris/nmail/issues/new) to ensure they
are properly tracked and get addressed.

Mailing List
------------
Feel free to subscribe at
[www.freelists.org/list/nmail-users](http://www.freelists.org/list/nmail-users)
and once subscribed, send messages to nmail-users [at] freelists [dot] org

Telegram Group
--------------
The Telegram group is available at
[https://t.me/nmailusers](https://t.me/nmailusers)


Security
========

nmail caches data locally to improve performance. Cached data can be encrypted
by setting by setting `cache_encrypt=1` in main.conf. Message databases are
then encrypted using OpenSSL AES256-CBC with a key derived from a random salt
and the email account password. Folder names are hashed using SHA256 (thus not
encrypted).

Storing the account password (`save_pass=1` in main.conf) is *not* secure.
While nmail encrypts the password, the key is trivial to determine from
the source code. Only store the password if measurements are taken to ensure
`~/.nmail/secret.conf` cannot by accessed by a third-party.


Configuration
=============

Aside from `main.conf` covered above, the following files can be used to
configure nmail.

~/.nmail/ui.conf
----------------
This configuration file controls the UI aspects of nmail. Default configuration
file (platform-dependent defaults are left empty below):

    attachment_indicator= 📎
    bottom_reply=0
    cancel_without_confirm=0
    colors_enabled=1
    compose_backup_interval=10
    compose_line_wrap=0
    delete_without_confirm=0
    full_header_include_local=0
    help_enabled=1
    invalid_input_notify=1
    key_back=,
    key_backward_kill_word=
    key_backward_word=
    key_cancel=KEY_CTRLC
    key_compose=c
    key_delete=d
    key_delete_line=KEY_CTRLK
    key_export=x
    key_ext_editor=KEY_CTRLE
    key_ext_html_preview=KEY_CTRLV
    key_ext_html_viewer=v
    key_ext_msg_viewer=w
    key_ext_pager=e
    key_filter_show_current_date=3
    key_filter_show_current_name=4
    key_filter_show_current_subject=5
    key_filter_show_has_attachments=2
    key_filter_show_unread=1
    key_filter_sort_reset=`
    key_find=/
    key_find_next=?
    key_forward=f
    key_forward_word=
    key_goto_folder=g
    key_import=i
    key_jump_to=j
    key_kill_word=
    key_move=m
    key_next_msg=n
    key_next_page=KEY_NPAGE
    key_open=.
    key_othercmd_help=o
    key_postpone=KEY_CTRLO
    key_prev_msg=p
    key_prev_page=KEY_PPAGE
    key_quit=q
    key_refresh=l
    key_reply_all=r
    key_reply_sender=R
    key_rich_header=KEY_CTRLR
    key_save_file=s
    key_search=/
    key_search_current_name=-
    key_search_current_subject==
    key_search_show_folder=\
    key_select_all=a
    key_select_item=KEY_SPACE
    key_send=KEY_CTRLX
    key_sort_date=#
    key_sort_has_attachments=@
    key_sort_name=$
    key_sort_subject=%
    key_sort_unread=!
    key_sync=s
    key_to_select=KEY_CTRLT
    key_toggle_full_header=h
    key_toggle_markdown_compose=KEY_CTRLN
    key_toggle_text_html=t
    key_toggle_unread=u
    localized_subject_prefixes=
    markdown_html_compose=0
    new_msg_bell=1
    persist_file_selection_dir=1
    persist_find_query=0
    persist_folder_filter=1
    persist_search_query=0
    persist_selection_on_sortfilter_change=1
    persist_sortfilter=1
    plain_text=1
    postpone_without_confirm=0
    quit_without_confirm=1
    respect_format_flowed=1
    rewrap_quoted_lines=1
    search_show_folder=0
    send_without_confirm=0
    show_embedded_images=1
    show_progress=1
    show_rich_header=0
    signature=0
    tab_size=8
    unread_indicator=N

### attachment_indicator

Controls which character to indicate that an email has attachments
(default: ` 📎`). For a more compact and plain layout one can use an ascii
character and omit the leading space character, example: `+`.

### bottom_reply

Controls whether to reply at the bottom of emails (default disabled).

### cancel_without_confirm

Allow canceling email compose without confirmation prompt (default disabled).

### colors_enabled

Enable terminal color output (default enabled).

### compose_backup_interval

Specify interval in seconds for local backups during compose (default 10).
If the system running nmail is unexpectedly shutdown while user is composing
an email, then upon next nmail startup any backuped compose message will be
automatically uploaded to the draft folder.
Setting this parameter to 0 disables local backups.

### compose_line_wrap

Specify how nmail shall wrap lines in outgoing emails. Supported options:

    0 = none (default)
    1 = using format=flowed
    2 = hardwrap at 72 chars width

### delete_without_confirm

Allow deleting emails (moving to trash folder) without confirmation
prompt (default disabled).

### full_header_include_local

While viewing full headers (by pressing `h`) nmail displays RFC 822 headers
by default. This parameter allows enabling nmail to also display local /
internal header fields, such as server timestamp. Default disabled.

### help_enabled

Show supported keyboard shortcuts at bottom of screen (default enabled).

### invalid_input_notify

Notify user when unsupported keyboard shortcuts are input (default enabled).

### key_

Keyboard bindings for various functions (default see above). The keyboard
bindings may be specified in the following formats:
- Ncurses macro (ex: `KEY_CTRLK`)
- Hex key code (ex: `0x22e`)
- Octal key code sequence (ex: `\033\177`)
- Plain-text lower-case ASCII (ex: `r`)

Octal key code sequence for a key combination can be determined for example
using the command `sed -n l` and pressing the combination, e.g. `alt-left`
and then `enter`. The keycode for non-printable keys is then displayed in
octal format in the terminal, for example:

    \033b$

The trailing `$` shall be ignored, and to comply with `nmail` config required
input format, the printable `b` needs to be converted to octal format. A quick
lookup using `man ascii` shows `b` is 142 in octal format. The resulting key
code to use in the config file is `\033\142`.

### localized_subject_prefixes

Email subjects are normalized (stripped of `re:`, `fwd:`) when sorting emails
by subject, and when replying to, or forwarding an email. By default only the
English prefixes `re` and `fwd?` (regex for `fwd` and `fw`) are removed. This
parameter allows extending the removal to other localized prefixes. Example
configuration for a Swedish user:

    localized_subject_prefixes=sv,vb

For a French user:

    localized_subject_prefixes=ref,tr

For a German user:

    localized_subject_prefixes=aw,wg

### markdown_html_compose

Default value for each new email, whether nmail shall enable markdown HTML
compose. I.e. whether nmail shall generate a text/html message part based on
processing the composed message as Markdown, when sending sending emails from
nmail. This can be overridden on a per-email basis by pressing CTRL-N when
editing an email (default disabled).

### new_msg_bell

Indicate new messages with terminal bell (default enabled).

### persist_file_selection_dir

Determines whether file selection view shall remember previous directory
(default enabled).

### persist_find_query

Controls whether to start with previous find query when performing repeated
find queries (default disabled).

### persist_folder_filter

Determines whether to persist move-to-folder list filter (default enabled).

### persist_search_query

Controls whether to start with previous search query when performing repeated
search queries (default disabled).

### persist_selection_on_sortfilter_change

Determines whether to keep current message list selection when
filtering/sorting mode is changed (default enabled).

### persist_sortfilter

Specifies whether each folder listing shall persist its filtering/sorting
mode (default enabled).

### plain_text

Determines whether showing plain text (vs. html converted to text) is
preferred. If the preferred email part is not present, nmail automatically
attempts to show the other. This option can be re-configured at run-time
by pressing `t` when viewing an email (default enabled).

### postpone_without_confirm

Allow postponing email compose without confirmation prompt (default disabled).

### quit_without_confirm

Allow exiting nmail without confirmation prompt (default enabled).

### respect_format_flowed

Specify whether nmail shall respect email line wrapping of format=flowed
type (default enabled).

### rewrap_quoted_lines

Control whether nmail shall rewrap quoted lines (default enabled).

### search_show_folder

Determines whether folder name should be shown in search results. This option
can be re-configured at run-time by pressing `\` when viewing search results
(default disabled).

### send_without_confirm

Allow sending email during compose without confirmation prompt (default
disabled).

### show_embedded_images

Determines whether to show embedded images in text/html part when viewing it
using external viewer; press right arrow when viewing a message to go to parts
view, and then select the text/html part and press right arrow again (default
enabled).

### show_progress

Determines whether nmail shall show a progress indication (in percentage) when
fetching emails (default enabled).

### show_rich_header

Determines whether to show rich headers (bcc field) during email compose. This
option can be re-configured in run-time by pressing `CTRL-R` when composing
an email (default disabled).

### signature

Determines whether to suffix emails with a signature (default disabled). When
enabled, nmail will use `~/.nmail/signature.txt` if present, or otherwise use
`~/.signature` for signature plain text content. When composing markdown
formatted emails, nmail will use `~/.nmail/signature.html` if present, for the
html part, and otherwise simply convert the plain text signature to html.

Note: For **custom html** signature to work properly, the plain text signature
should not be present more than once in the composed message, thus a very short
plain text signature may not be ideal.

Example signature files: [signature.txt](/doc/signature.txt),
[signature.html](/doc/signature.html)

### tab_size

Tabs are expanded to spaces when viewed in nmail. This parameter controls the
space between tab stops (default 8).

### unread_indicator

Controls which character to indicate that an email is unread (default: `N`).
For a more graphical interface, an emoji such as `✉` can be used.


~/.nmail/colors.conf
--------------------
This configuration file controls the configurable colors of nmail. For this
configuration to take effect, `colors_enabled=1` must be set in
`~/.nmail/ui.conf`.

Example color config files are provided in `/usr/local/share/nmail/themes`
and can be used by overwriting `~/.nmail/colors.conf`.

### Htop style theme

This color theme is similar to htop's default, see screenshot below with
nmail and htop.

![screenshot nmail htop style theme](/doc/screenshot-nmail-htop-theme.png)

To use this config:

    cp /usr/local/share/nmail/themes/htop-style.conf ~/.nmail/colors.conf

### Manual configuration

Alternatively one may manually edit `colors.conf`. Colors may
be specified using standard palette names (`black`, `red`, `green`, `yellow`,
`blue`, `magenta`, `cyan`, `white`, `gray`, `bright_red`, `bright_green`,
`bright_yellow`, `bright_blue`, `bright_magenta`, `bright_cyan` and
`bright_white`) or using integer palette numbers (`0`, `1`, `2`, etc).

To use default terminal color, leave the color empty or set it to `normal`.
To use inverted / reverse color set both `fg` and `bg` values to `reverse`.

For terminals supporting custom palettes it is also possible to specify colors
using six digit hex format with `0x` prefix, e.g. `0xa0a0a0`. For each item
background `_bg` and foreground `_fg` can be specified. Default
configuration file:

    color_dialog_bg=reverse
    color_dialog_fg=reverse
    color_help_desc_bg=
    color_help_desc_fg=
    color_help_keys_bg=reverse
    color_help_keys_fg=reverse
    color_highlighted_text_bg=reverse
    color_highlighted_text_fg=reverse
    color_quoted_text_bg=
    color_quoted_text_fg=gray
    color_regular_text_bg=
    color_regular_text_fg=
    color_selected_item_bg=
    color_selected_item_fg=gray
    color_top_bar_bg=reverse
    color_top_bar_fg=reverse

### color_dialog

User prompt dialogs and notifications at bottom of the screen, just above the
help bar.

### color_help_desc

Help shortcut description texts at bottom of the screen, i.e. `Compose` in
`C Compose`.

### color_help_keys

Help shortcut key binding texts at bottom of the screen, i.e. `C` in
`C Compose`.

### color_highlighted_text

Highlighted text, such as current message in message view, current folder in
folder list, text strings found in message find, etc.

### color_quoted_text

Quoted message text (lines starting with `>`).

### color_regular_text

Default text color.

### color_selected_item

Selected messages in message list view.

### color_top_bar

Top / title bar.


~/.nmail/auth.conf
------------------
This configuration file allows users to set up custom OAuth 2.0 client id and
client secret. If not specified, nmail uses its own application id and secret.
Default configuration file:

    oauth2_client_id=
    oauth2_client_secret=

### oauth2_client_id

Custom OAuth 2.0 client id.

### oauth2_client_secret

Custom OAuth 2.0 client secret.


Email Service Providers
=======================

Gmail Prerequisites
-------------------
Gmail prevents IMAP access by default.

In order to enable IMAP access go to the Gmail web interface - typically
[mail.google.com](https://mail.google.com) - and navigate to 
`Settings -> Forwarding and POP/IMAP -> IMAP access` and select: `Enable IMAP`

### Password Authentication
Gmail prevents password authentication by default. To enable
password-authenticated IMAP access, one must either set up an "app password"
or enable "less secure app access".

To set up an "app password", navigate to
[https://myaccount.google.com/apppasswords](https://myaccount.google.com/apppasswords)
and select app "Mail" and an appropriate device, e.g. "Mac", then click
Generate.

To enable "less secure app access", go to
[myaccount.google.com/security](https://myaccount.google.com/security) and
under `Less secure app access` click `Turn on access (not recommended)`.

### OAuth 2.0 Authentication
Google OAuth 2.0 application review has not yet been requested for nmail, and
as such users need to request an invitation to use this. Please send an email
to `d99kris at gmail dot com` with subject `nmail google oauth2 invite` from
the google account address you would like to be invited.

Alternatively a user may set up their own OAuth 2.0 application with Google
and configure `~/.nmail/auth.conf` accordingly.


Accessing Email Cache using Other Email Clients
===============================================

The nmail message cache may be exported to the Maildir format using the
following command:

    nmail --export ~/Maildir

A basic `~/.muttrc` config file for reading the exported Maildir in `mutt`:

    set mbox_type=Maildir
    set spoolfile="~/Maildir"
    set folder="~/Maildir"
    set mask=".*"

Note: nmail is not designed for working with other email clients, this export
option is mainly available as a data recovery option in case access to an
email account is lost, and one needs a local Maildir archive to import into
a new email account. Such import is not supported by nmail, but is supported
by some other email clients, like Thunderbird.


Technical Details
=================

Third-party Libraries
---------------------
nmail is implemented in C++. Its source tree includes the source code of the
following third-party libraries:

- [apathy](https://github.com/dlecocq/apathy) -
  Copyright 2013 Dan Lecocq - [MIT License](/ext/apathy/LICENSE)
- [cereal](https://github.com/USCiLab/cereal) -
  Copyright 2014 Randolph Voorhies, Shane Grant - [BSD-3 License](/ext/cereal/LICENSE)
- [cxx-prettyprint](https://github.com/louisdx/cxx-prettyprint) -
  Copyright 2010 Louis Delacroix - [Boost License](/ext/cxx-prettyprint/LICENSE_1_0.txt)
- [sqlite_modern_cpp](https://github.com/SqliteModernCpp/sqlite_modern_cpp) -
  Copyright 2017 aminroosta - [MIT License](/ext/sqlite_modern_cpp/License.txt)

Code Formatting
---------------
Uncrustify is used to maintain consistent source code formatting, example:

    uncrustify -c etc/uncrustify.cfg --replace --no-backup src/*.cpp src/*.h


License
=======

nmail is distributed under the MIT license. See [LICENSE](/LICENSE) file.


Keywords
========

alternative to alpine, command line, console-based, email client, linux, macos, ncurses,
terminal-based.
