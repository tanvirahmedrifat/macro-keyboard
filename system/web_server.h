#ifndef WEB_SERVER_H
#define WEB_SERVER_H

void WebServer_Init();
void WebServer_Stop();
bool WebServer_RebootRequested(); // returns true if reboot was requested via web

#endif
