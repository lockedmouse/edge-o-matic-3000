#include "../include/WebSocketSecureHelper.h"
#include "../include/WebSocketHelper.h"

// Include certificate data
#include "../include/ssl/cert.h"
#include "../include/ssl/private_key.h"

namespace WebSocketSecureHelper {
  void setup() {
    // Create an SSL certificate object from the files included above ...
    cert = new SSLCert(
        example_crt_DER, example_crt_DER_len,
        example_key_DER, example_key_DER_len
    );

    // ... and create a server based on this certificate.
    // The constructor has some optional parameters like the TCP port that should be used
    // and the max client count. For simplicity, we use a fixed amount of clients that is bound
    // to the max client count.
    secureServer = new HTTPSServer(cert, 443, MAX_CLIENTS);

    // Initialize the slots
    for (int i = 0; i < MAX_CLIENTS; i++) activeClients[i] = nullptr;

    // For every resource available on the server, we need to create a ResourceNode
    // The ResourceNode links URL and HTTP method to a handler function
    ResourceNode *nodeRoot = new ResourceNode("/", "GET", &handleRoot);
    secureServer->registerNode(nodeRoot);

    // Check CORS here. Some browsers appreciate proper preflights before connecting
    // to WebSockets. Security and all, yanno?
    ResourceNode *corsNode = new ResourceNode("/*", "OPTIONS", &handleCors);
    secureServer->registerNode(corsNode);

    // The websocket handler can be linked to the server by using a WebsocketNode:
    // (Note that the standard defines GET as the only allowed method here,
    // so you do not need to pass it explicitly)
    WebsocketNode *chatNode = new WebsocketNode("/", &RemoteHandler::create);
    secureServer->registerNode(chatNode);

    // Finally, add the 404 not found node to the server.
    // The path is ignored for the default node.
    ResourceNode *node404 = new ResourceNode("", "GET", &handle404);
    secureServer->setDefaultNode(node404);

    Serial.println("Starting server...");
    secureServer->start();
    if (secureServer->isRunning()) {
      Serial.print("Server ready. Open the following URL in multiple browser windows to start chatting: https://");
      Serial.println(WiFi.localIP());
    }
  }

  void loop() {
    // This call will let the server do its work
    secureServer->loop();
  }

  void send(int num, String payload) {
    if (num >= 0) {
      RemoteHandler *c = (RemoteHandler*) activeClients[num];
      if (c != nullptr)
        c->sendText(payload);
    } else {
      for (int i = 0; i < MAX_CLIENTS; i++) {
        send(i, payload);
      }
    }
  }

  void handle404(HTTPRequest *req, HTTPResponse *res) {
    // Discard request body, if we received any
    // We do this, as this is the default node and may also server POST/PUT requests
    req->discardRequestBody();

    // Set the response status
    res->setStatusCode(404);
    res->setStatusText("Not Found");

    // Set content type of the response
    res->setHeader("Content-Type", "text/html");

    // Write a tiny HTTP page
    res->println("<!DOCTYPE html>");
    res->println("<html>");
    res->println("<head><title>Not Found</title></head>");
    res->println("<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>");
    res->println("</html>");
  }

  // The following HTML code will present the chat interface.
  void handleRoot(HTTPRequest *req, HTTPResponse *res) {
    res->setHeader("Content-Type", "text/html");

    // TODO Redirect here

    res->println("<!DOCTYPE html>");
    res->println("<html>");
    res->println("<head><title>Redirecting...</title></head>");
    res->println("<body><h1>Redirecting...</h1><p>You are now being redirected to the Web UI for the Edge-o-Matic.</p></body>");
    res->println("</html>");
  }

  void handleCors(HTTPRequest *req, HTTPResponse *res) {
    Serial.println(">> CORS here...");
    res->setHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    res->setHeader("Access-Control-Allow-Origin", "*");
    res->setHeader("Access-Control-Allow-Headers", "*");
  }


  // In the create function of the handler, we create a new Handler and keep track
  // of it using the activeClients array
  WebsocketHandler *RemoteHandler::create() {
    Serial.println("Creating new chat client!");
    WebsocketHandler *handler = new RemoteHandler();
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (activeClients[i] == nullptr) {
        activeClients[i] = handler;
        break;
      }
    }
    return handler;
  }

  int RemoteHandler::getIndex() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (activeClients[i] == this) {
        return i;
      }
    }
    return -1;
  }

  // When the websocket is closing, we remove the client from the array
  void RemoteHandler::onClose() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (activeClients[i] == this) {
        activeClients[i] = nullptr;
      }
    }
  }

  // Finally, passing messages around. If we receive something, we send it to all
  // other clients
  void RemoteHandler::onMessage(WebsocketInputStreambuf *inbuf) {
    // Get the input message
    std::ostringstream ss;
    std::string msg;
    ss << inbuf;
    msg = ss.str();

    WebSocketHelper::onMessage(this->getIndex(), msg.c_str());
  }

  void RemoteHandler::sendText(String payload) {
    this->send(payload.c_str(), SEND_TYPE_TEXT);
  }
}