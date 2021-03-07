#!/usr/bin/env python3

from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse
import json
import os
import requests
import stat
import sys
import threading
import time
import urllib.parse
import webbrowser

version = "1.01"
base_url = "https://accounts.google.com"
pathServed = ""


def show_help():
    print("oauth2nmail.py is a utility tool for handling OAuth2 authentication and")
    print("token renewals. Primarily written for use by nmail email client.")
    print("Parameters must be passed using environment variables, see examples below.")
    print("")
    print("Usage: oauth2nmail.py --generate")
    print("   or: oauth2nmail.py --refresh")
    print("   or: oauth2nmail.py --help")
    print("   or: oauth2nmail.py --version")
    print("")
    print("Options:")
    print("   -g, --generate    perform authentication and generate refresh/access tokens")
    print("   -r, --refresh     refresh access token using refresh token")
    print("   -h, --help        display this help and exit")
    print("   -v, --version     output version information and exit")
    print("")
    print("Return values:")
    print("   0                 success")
    print("   1                 syntax / usage error")
    print("   2                 authentication timeout")
    print("   3                 user permission not granted")
    print("   4                 http request network error")
    print("   5                 http request returned non-200")
    print("   6                 access token not available")
    print("   130               keyboard interrupt (ctrl-c)")
    print("")
    print("Examples:")
    print("   OAUTH2_TYPE=\"gmail-oauth2\" OAUTH2_CLIENT_ID=\"9\" OAUTH2_CLIENT_SECRET=\"j\" \\")
    print("   OAUTH2_TOKEN_STORE=\"${HOME}/.nmail/oauth2.tokens\" oauth2nmail.py -g")
    print("")
    print("   OAUTH2_TYPE=\"gmail-oauth2\" OAUTH2_CLIENT_ID=\"9\" OAUTH2_CLIENT_SECRET=\"j\" \\")
    print("   OAUTH2_TOKEN_STORE=\"${HOME}/.nmail/oauth2.tokens\" oauth2nmail.py -r")
    print("")
    print("Report bugs at https://github.com/d99kris/nmail")
    print("")


def show_version():
    print("oauth2nmail.py v" + version)
    print("")
    print("Copyright (c) 2021 Kristofer Berggren")
    print("")
    print("oauth2nmail.py is distributed under the MIT license.")
    print("")
    print("Written by Kristofer Berggren.")


def url_params(params):
    param_list = []
    for param in params.items():
        param_list.append("%s=%s" % (param[0], urllib.parse.quote(param[1], safe="~-._")))
    return "&".join(param_list)


def save_tokens(tokenStore, tokenItems):
    with open(tokenStore, "w") as tokenFile:
        for key, value in tokenItems:
            tokenFile.write(str(key) + "=" + str(value) + "\n")


def read_tokens(tokenStore):
    tokens = {}
    if os.path.isfile(tokenStore):
        with open(tokenStore, "r") as tokenFile:
            tokenLines = tokenFile.read().splitlines()
            for tokenLine in tokenLines:
                tokenWords = tokenLine.split("=", 1)
                tokens[str(tokenWords[0])] = tokenWords[1]
    return tokens


def generate(provider, client_id, client_secret, token_store):
    localHostName = "localhost"
    localHostPort = 6880
    redirectPage = "/oauth2nmail.py"
    redirectUri = "http://" + localHostName + ":" + str(localHostPort) + redirectPage

    # open default web browser for user authentication
    params = {}
    params["client_id"] = client_id
    params["redirect_uri"] = redirectUri
    params["scope"] = "openid profile email https://mail.google.com/"
    params["response_type"] = "code"
    url = base_url + "/o/oauth2/auth?" + url_params(params)
    webbrowser.open(url, new = 0, autoraise = True)

    # local web server handling redirect
    class LocalServer(BaseHTTPRequestHandler):
        def log_message(self, format, *args):
            return

        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            if self.path.startswith(redirectPage):
                self.wfile.write(bytes("<html><head><title>Nmail Oauth2</title></head>", "utf-8"))
                self.wfile.write(bytes("<body><h2>Authentication successful</h2>", "utf-8"))
                self.wfile.write(bytes("<p>You may close this browser window now.</p>", "utf-8"))
                self.wfile.write(bytes("</body></html>", "utf-8"))
                global pathServed
                pathServed = self.path

    localServer = HTTPServer((localHostName, localHostPort), LocalServer)
    thread = threading.Thread(target = localServer.serve_forever)
    thread.daemon = True
    thread.start()
    secs = 0
    while not pathServed and secs < 60:
        time.sleep(1)
        secs = secs + 1

    if not pathServed:
        sys.stderr.write("authentication timeout\n")
        return 2

    query = urlparse(pathServed).query
    queryDict = dict(qc.split("=") for qc in query.split("&"))
    if "code" not in queryDict:
        sys.stderr.write("user did not grant permission\n")
        return 3
    
    code = queryDict["code"]

    # exchange auth code for access and refresh token
    convParams = {}
    convParams["client_id"] = client_id
    convParams["client_secret"] = client_secret
    convParams["code"] = code
    convParams["redirect_uri"] = redirectUri
    convParams["grant_type"] = "authorization_code"
    convUrl = base_url + "/o/oauth2/token"
    try:
        convResponse = requests.post(convUrl, convParams)
    except Exception as e:
        sys.stderr.write("token request http post failed " + str(e) + "\n")
        return 4

    if convResponse.status_code != 200:
        sys.stderr.write("token request failed " + str(convResponse) + "\n")
        return 5

    # save tokens
    jsonResponse = json.loads(convResponse.text)
    save_tokens(token_store, jsonResponse.items())
    tokens = read_tokens(token_store)

    # ensure access_token is present
    if "access_token" in tokens:
        access_token = tokens["access_token"]
    else:
        sys.stderr.write("access_token not available\n")
        return 6

    # request email address
    emailParams = {}
    emailParams["access_token"] = access_token
    try:
        emailResponse = requests.get("https://openidconnect.googleapis.com/v1/userinfo", emailParams)
    except Exception as e:
        sys.stderr.write("email address get request failed " + str(e) + "\n")
        return 4

    if emailResponse.status_code != 200:
        sys.stderr.write("email address request failed " + str(emailResponse) + "\n")
        return 5

    # save user info (email, name)
    jsonEmailResponse = json.loads(emailResponse.text)
    emailItems = jsonEmailResponse.items()
    for key, value in emailItems:
        if key == "email" or key == "name":
            tokens[key] = value
    
    save_tokens(token_store, tokens.items())
    return 0


def refresh(provider, client_id, client_secret, token_store):
    # read token store
    tokens = read_tokens(token_store)

    # ensure refresh_token is present
    if "refresh_token" in tokens:
        refresh_token = tokens["refresh_token"]
    else:
        sys.stderr.write("refresh_token not set in " + token_store + "\n")
        return 1
    
    # use refresh code to request new access token
    refreshParams = {}
    refreshParams["client_id"] = client_id
    refreshParams["client_secret"] = client_secret
    refreshParams["refresh_token"] = refresh_token
    refreshParams["grant_type"] = "refresh_token"
    refreshUrl = base_url + "/o/oauth2/token"
    try:
        refreshResponse = requests.post(refreshUrl, refreshParams)
    except Exception as e:
        sys.stderr.write("token refresh http post failed " + str(e) + "\n")
        return 4

    if refreshResponse.status_code != 200:
        sys.stderr.write("token refresh failed " + str(refreshResponse) + "\n")
        return 5

    # save received tokens
    jsonRefreshResponse = json.loads(refreshResponse.text)
    refreshedTokens = jsonRefreshResponse.items()
    for key, value in refreshedTokens:
        tokens[key] = value

    save_tokens(token_store, tokens.items())
    return 0


def main(argv):
    # Global setting
    os.umask(stat.S_IRWXG | stat.S_IRWXO)

    # Process arguments
    if len(sys.argv) != 2:
        show_help()
        sys.exit(1)
    elif (sys.argv[1] == "--help") or (sys.argv[1] == "-h"):
        show_help()
        sys.exit(0)
    elif (sys.argv[1] == "--version") or (sys.argv[1] == "-v"):
        show_version()
        sys.exit(0)

    # Determine operation
    isGenerate = (sys.argv[1] == "--generate") or (sys.argv[1] == "-g")
    isRefresh = (sys.argv[1] == "--refresh") or (sys.argv[1] == "-r")

    if not isGenerate and not isRefresh:
        show_help()
        sys.exit(1)

    # Read required environment variables
    provider = os.getenv("OAUTH2_TYPE")
    if not provider:
        sys.stderr.write("env OAUTH2_TYPE not set\n")
        sys.exit(1)
    elif provider != "gmail-oauth2":
        sys.stderr.write("OAUTH2_TYPE provider " + provider + " not supported\n")
        sys.exit(1)

    client_id = os.getenv("OAUTH2_CLIENT_ID")
    if not client_id:
        sys.stderr.write("env OAUTH2_CLIENT_ID not set\n")
        sys.exit(1)

    client_secret = os.getenv("OAUTH2_CLIENT_SECRET")
    if not client_secret:
        sys.stderr.write("env OAUTH2_CLIENT_SECRET not set\n")
        sys.exit(1)

    token_store = os.getenv("OAUTH2_TOKEN_STORE")
    if not token_store:
        sys.stderr.write("env OAUTH2_TOKEN_STORE not set\n")
        sys.exit(1)

    # Perform requested operation
    if isGenerate:
        rv = generate(provider, client_id, client_secret, token_store)
        sys.exit(rv)
    elif isRefresh:
        rv = refresh(provider, client_id, client_secret, token_store)
        sys.exit(rv)


if __name__ == "__main__":
    main(sys.argv)
