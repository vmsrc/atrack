# OruxMaps/MapMyTracks based GPS Activity Tracker

Self-hosted online location sharing.  
 - [OruxMaps](https://www.oruxmaps.com/cs/en/) compatible.
 - [Leaflet](https://leafletjs.com/) based web map.
 - [Map My Tracks API (v1)](https://github.com/MapMyTracks/api-v1) protocol.  
 - FastCGI server side implementation.

## Installing
* Build FastCGI module using provided Makefile or compile manually:  
```
gcc -Wall -lfcgi -O2 -s -o atrack.fcgi atrack.c
```
* Web server configuration - sample for lighttpd:  
```
server.modules += ( "mod_fastcgi" )

fastcgi.server = (
  "/base/atrack_url/" => ((
    "bin-path" => "/path/to/atrack.fcgi",
    "socket" => "/tmp/atrack-fcgi.sock",
    "check-local" => "disable",
    "max-procs" => 1,
  ))
)
```
*Note: the `"/base/atrack_url/"` path could be simply `"/atrack/"` - referred below as "the simple case".*  
* Create database folder, accessible via http, by adding `"_data"`suffix to the base URL prefix above:  
`/var/www/html/base/atrack_url_data`  
*In the simple case: `/var/www/html/atrack_data`*  
* Copy and rename if necessary the `"atrack.html"` file next to the database folder created above:  
`/var/www/html/base/atrack_url.html`  
*In the simple case:  `/var/www/html/atrack.html ` - without renamning*  
## OruxMaps configuration
Configure remote server URL:  
`http://mydomain.com/base/atrack_url/ACTIVITY_NAME`  
*In the simple case: `http://mydomain.com/atrack/ACTIVITY_NAME`*  
## View activity
Open the (optionally renamed) HTML page in web browser:  
`http://mydomain.com/base/atrack_url.html`  
*In the simple case: `http://mydomain.com/atrack.html`*  
Enter the ACTIVITY_NAME and press START.  
