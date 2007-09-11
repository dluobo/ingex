Installation
------------

Requirements:
* apache2 web server, with static html documents in /srv/www/htdocs and cgi in 
/srv/www/cgi-bin
* perl CGI, CGI::Pretty, Text::Template, DBI, PostgreSQL DBD modules

You can install these modules using YaST or via CPAN as follows:
eg. perl -MCPAN -e 'install Text::Template'


1) edit the configuration file "conf/ingex.conf" to match your system setup
2) run "sudo install/install.sh" to install the files.



Usage
-----

Open your web browser to the page http://<host name>/cgi-bin/ingex/index.pl




