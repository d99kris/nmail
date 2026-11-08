Tips
====

Viewing Attachments in the Terminal
-----------------------------------
Attachments such as PDF and Word documents can be viewed as text in the
terminal. In the message parts view, select the part and press `v` to open it
in the html viewer (see `html_viewer_cmd`). With the default configuration
nmail uses `w3m` together with `lesspipe`, which converts supported file types
to text (PDF via `pdftotext` from poppler, other formats depending on the
converters available to lesspipe).

Install the tools:

macOS

    brew install w3m lesspipe poppler

Debian-based (Ubuntu, Raspbian, etc)

    sudo apt install w3m less poppler-utils
