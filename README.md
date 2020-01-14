nmail - ncurses mail
====================

| **Linux + Mac** |
|---------|
| [![Build status](https://travis-ci.com/d99kris/nmail.svg?branch=master)](https://travis-ci.com/d99kris/nmail) |

nmail is a console-based email client for Linux and macOS with a user interface
similar to alpine / pine.

![screenshot](/doc/screenshot.png) 

Features
--------
- Support for IMAP and SMTP protocols
- Local cache using AES256-encrypted custom Maildir format
- Multi-threaded (email fetch and send done in background)
- Address book auto-generated based on email messages
- Viewing HTML emails
- Opening/viewing attachments in external program
- Simple setup wizard for GMail and Outlook/Hotmail
- Familiar UI for alpine / pine users
- Compose message using external editor ($EDITOR)
- Saving and continuing draft messages

Planned features
----------------
- Email search

Not planned
-----------
- Multiple email accounts in a single session
- Special handling for GMail labels
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

    -h, --help
        display this help and exit

    -o, --offline
        run in offline mode

    -s, --setup <SERV>
        setup wizard for specified service, supported services: gmail, outlook

    -v, --version
        output version information and exit

Configuration files:

    ~/.nmail/main.conf
        configures mail account and general setings.

    ~/.nmail/ui.conf
        customizes UI settings.

Examples:

    nmail -s gmail
        setup nmail for a gmail account


Supported Platforms
===================

nmail is developed and tested on Linux and macOS. Current version has been
tested on:

- macOS 10.14 Mojave
- Ubuntu 18.04 LTS


Build / Install
===============

Linux / Ubuntu
--------------

**Dependencies**

    sudo apt install git cmake libetpan-dev libssl-dev libncurses-dev help2man lynx

**Source**

    git clone https://github.com/d99kris/nmail && cd nmail

**Build**

    mkdir -p build && cd build && cmake .. && make -s

**Install**

    sudo make install

macOS
-----

**Dependencies**

    brew install cmake libetpan openssl ncurses help2man lynx

**Source**

    git clone https://github.com/d99kris/nmail && cd nmail

**Build**

    mkdir -p build && cd build && cmake .. && make -s

**Install**

    make install


Getting Started
===============

Gmail
-----
Use the setup wizard to set up nmail for the account. Example (replace
example@gmail.com with your actual gmail address):

    $ nmail -s gmail
    Email: example@gmail.com
    Name: Firstname Lastname
    Save password (y/n): y
    Password: 

Outlook (and Hotmail)
---------------------
Use the setup wizard to set up nmail for the account. Example (replace
example@hotmail.com with your actual outlook / hotmail address):

    $ nmail -s outlook
    Email: example@hotmail.com
    Name: Firstname Lastname
    Save password (y/n): y
    Password: 

Other Email Providers
---------------------
Run nmail once in order for it to automatically generate the default config
file:

    $ nmail

Then open the config file `~/.nmail/main.conf` in your favourite text editor
and fill out the empty fields (except `pass`), example:

    address=example@example.com
    cache_encrypt=1
    drafts=Drafts
    ext_viewer_cmd=open -Wn
    html_convert_cmd=/usr/local/bin/lynx -dump
    imap_host=imap.example.com
    imap_port=993
    inbox=INBOX
    name=Firstname Lastname
    pass=
    prefetch_level=2
    save_pass=1
    smtp_host=smtp.example.com
    smtp_port=465
    trash=Trash
    user=example@example.com
    verbose_logging=0

The field `pass` shall be left empty. The next time nmail is started it will
prompt for password. If having configured nmail to save the password
`save_pass=1` the field `pass` will be automatically populated with the
password encrypted (refer to Security section below for details).


Multiple Email Accounts
=======================

nmail does currently not support multiple email accounts (in a single session).
It is however possible to run multiple nmail instances in parallel with
different config directories (and thus different email accounts), but it will
be just that - multiple instances - each in its own terminal. To facilitate
such usage one can set up aliases for accessing different accounts, e.g.:

    alias gm='nmail -d ${HOME}/.nmail-gm' # gmail
    alias hm='nmail -d ${HOME}/.nmail-hm' # hotmail


Pre-fetching
============

nmail pre-fetches messages from server based on the `prefetch_level` config
setting. The following levels are supported:

    0 = no pre-fetching, messages are retrieved when viewed
    1 = pre-fetching of currently selected message
    2 = pre-fetching of all messages in current folder view (default)
    3 = pre-fetching of all messages in all folders, i.e. full sync


Troubleshooting
===============

If any issues are observed, try running nmail with verbose logging

    nmail --verbose

and provide a copy of ~/.nmail/log.txt when reporting the issue. The
preferred way of reporting issues and asking questions is by opening 
[a Github issue](https://github.com/d99kris/nmail/issues/new).

Verbose logging can also be enabled by setting `verbose_logging=1` in
`~/.nmail/main.conf`.


Email List
==========
An email list is available for users to discuss nmail usage and related topics.
Feel free to [subscribe](http://www.freelists.org/list/nmail-users) and send
messages to [nmail-users@freelists.org](mailto:nmail-users@freelists.org).

Bug reports, feature requests and usage questions directed at the nmail
maintainer(s) should however be reported using
[Github issues](https://github.com/d99kris/nmail/issues/new) to ensure they
are properly tracked and get addressed.


Security
========

nmail caches data locally to improve performance. By default the cached
data is encrypted (`cache_encrypt=1` in main.conf). Messages are encrypted
using OpenSSL AES256-CBC with a key derived from a random salt and the
email account password. Folder names are hashed using SHA256 (thus
not encrypted).

Using the command line tool `openssl` it is possible to decrypt locally
cached messages / headers. Example (enter email account password at prompt):

    openssl enc -d -aes-256-cbc -md sha1 -in ~/.nmail/cache/imap/B5/152.eml

Storing the account password (`save_pass=1` in main.conf) is *not* secure.
While nmail encrypts the password, the key is trivial to determine from
the source code. Only store the password if measurements are taken to ensure
`~/.nmail/main.conf` cannot by accessed by a third-party.


Configuration
=============

Aside from `main.conf` covered above, the following file can be used to
configure nmail.

~/.nmail/ui.conf
----------------
This configuration file controls the UI aspects of nmail. Default configuration
file:

    help_enabled=1
    key_address_book=KEY_CTRLT
    key_back=,
    key_cancel=KEY_CTRLC
    key_compose=c
    key_delete=d
    key_delete_line=KEY_CTRLK
    key_external_editor=KEY_CTRLE
    key_forward=f
    key_goto_folder=g
    key_move=m
    key_next_msg=n
    key_open=.
    key_postpone=KEY_CTRLO
    key_prev_msg=p
    key_quit=q
    key_refresh=l
    key_reply=r
    key_save_file=s
    key_send=KEY_CTRLX
    key_toggle_text_html=t
    key_toggle_unread=u
    new_msg_bell=1
    persist_folder_filter=1
    plain_text=1
    show_progress=1


Technical Details
=================

nmail is implemented in C++. Its source tree includes the source code of the
following third-party libraries:

- [apathy](https://github.com/dlecocq/apathy) - MIT License
- [cxx-prettyprint](https://github.com/louisdx/cxx-prettyprint) - Boost License


License
=======
nmail is distributed under the MIT license. See [LICENSE](/LICENSE) file.


Keywords
========
command line, console based, linux, macos, email client, ncurses, terminal,
alternative to alpine.

