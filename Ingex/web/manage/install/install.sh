#!/bin/sh

# very simple installation
# TODO: site specific settings, etc.

mkdir -p /srv/www/cgi-bin/ingex

cp cgi/*.pl /srv/www/cgi-bin/ingex
chmod 555 /srv/www/cgi-bin/ingex/*.pl

cp modules/*.pm /srv/www/cgi-bin/ingex
chmod 444 /srv/www/cgi-bin/ingex/*.pm

cp templates/*.tmpl /srv/www/cgi-bin/ingex
chmod 444 /srv/www/cgi-bin/ingex/*.tmpl

cp conf/*.conf /srv/www/cgi-bin/ingex
chmod 444 /srv/www/cgi-bin/ingex/*.conf



mkdir -p /srv/www/htdocs/ingex

cp styles/*.css /srv/www/htdocs/ingex
chmod 444 /srv/www/htdocs/ingex/*.css


