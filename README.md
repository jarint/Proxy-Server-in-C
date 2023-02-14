# Proxy-Server-in-C
A web proxy server built in C

DESCRIPTION: This program uses basic socket functionality in C to create a proxy server that can dynamically censor HTTP webpages using a keyword that it matches to the header of the webpage. The user specifies the portnumber as the second command line argument when running the program. It uses the HTTP GET method to fetch HTTP webpages from the server and sends them back to the client.
