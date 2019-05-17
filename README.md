nmail - ncurses mail
====================

| **Mac** |
|---------|
| [![Build status](https://travis-ci.org/d99kris/nmail.svg?branch=master)](https://travis-ci.org/d99kris/nmail) |

nmail is a console-based email client for Linux and macOS with a user interface
similar to alpine / pine.

![screenshot](/doc/screenshot.png) 


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

    sudo apt install git cmake libetpan-dev libssl-dev libncurses-dev help2man

**Source**

    git clone https://github.com/d99kris/nmail && cd nmail

**Build**

    mkdir -p build && cd build && cmake .. && make -s

**Install**

    sudo make install

macOS
-----

**Dependencies**

    brew install cmake libetpan openssl ncurses help2man

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
    html_convert_cmd=/usr/local/bin/lynx -dump
    imap_host=imap.example.com
    imap_port=993
    inbox=INBOX
    name=Firstname Lastname
    pass=
    save_pass=1
    smtp_host=smtp.example.com
    smtp_port=465
    trash=
    user=example@example.com

The field `pass` shall be left empty. The next time nmail is started it will
prompt for password. If having configured nmail to save the password
`save_pass=1` the field `pass` will be automatically populated with the
password encrypted (refer to Security section below for details).


Troubleshooting
===============

If any issues are observed, try running nmail with verbose logging

    nmail --verbose

and provide a copy of ~/.nmail/log.txt when reporting the issue. The
preferred way of reporting issues and asking questions is by opening 
[a Github issue](https://github.com/d99kris/nmail/issues/new). 


Security
========

nmail caches data locally to improve performance. By default the cached
data is encrypted (`cache_encrypt=1` in main.conf). Messages are encrypted
using OpenSSL AES256-CBC with a key derived from a random salt and the
email account password. Folder names are hashed using SHA256 (thus
not encrypted).

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
    key_forward=f
    key_goto_folder=g
    key_move=m
    key_next_msg=n
    key_open=.
    key_prev_msg=p
    key_quit=q
    key_refresh=l
    key_reply=r
    key_send=KEY_CTRLX
    key_toggle_text_html=t
    key_toggle_unread=u
    persist_folder_filter=1
    plain_text=1


Technical Details
=================

nmail is implemented in C++. Its source tree includes the source code of the
following third-party libraries:

- [apathy](https://github.com/dlecocq/apathy) - MIT License


License
=======
nmail is distributed under the MIT license. See [LICENSE](/LICENSE) file.


Keywords
========
command line, console based, linux, macos, email client, ncurses, terminal,
alternative to alpine.
