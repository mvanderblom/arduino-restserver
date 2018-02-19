#include "RestServer.h"

RestServer::RestServer(EthernetServer& server) :
		server_(server), routesIndex_(0), bufferIndex_(0) {
}

void RestServer::run() {
	client_ = server_.available();
	if (client_) {
		JSON_START()
		;
		// Check the received request and process it
		check();
		bufferIndex_--;
		JSON_CLOSE()
		;

		// Send data for the client
		send(8, 0);

		// Stop the client connection
		client_.stop();

		// Necessary resets
		reset();
	}
}

void RestServer::reset() {
	// Reset buffer
	memset(&buffer_[0], 0, sizeof(buffer_));

	// Reset buffer index
	bufferIndex_ = 0;
}

void RestServer::addRoute(char* method, char* route,
		void (*f)(char* params, char* body, uint8_t bodySize)) {
//void RestServer::addRoute(char * method, char * route, void (*f)(char * params, char * body) ) {
	routes_[routesIndex_].method = method;
	routes_[routesIndex_].name = route;
	routes_[routesIndex_].callback = f;
	routesIndex_++;
}

void RestServer::addToBuffer(char* value) {
	for (int i = 0; i < strlen(value); i++) {
		buffer_[bufferIndex_ + i] = value[i];
	}
	bufferIndex_ = bufferIndex_ + strlen(value);
}

void RestServer::addData(char* name, char * value) {
	char bufferAux[255] = { 0 };
	uint16_t idx = 0;

	// Format the data as:
	// "name":"value",
	bufferAux[idx++] = '"';
	for (int i = 0; i < strlen(name); i++) {
		bufferAux[idx++] = name[i];
	}
	bufferAux[idx++] = '"';

	bufferAux[idx++] = ':';
	bufferAux[idx++] = '"';
	for (int i = 0; i < strlen(value); i++) {
		bufferAux[idx++] = value[i];
	}
	bufferAux[idx++] = '"';
	bufferAux[idx++] = ',';

	addToBuffer(bufferAux);
}

void RestServer::addData(char* name, uint16_t value) {
	char number[10];
	itoa(value, number, 10);

	addData(name, number);
}

void RestServer::addData(char* name, int value) {
	char number[10];
	itoa(value, number, 10);

	addData(name, number);
}

void RestServer::send(uint8_t chunkSize, uint8_t delayTime) {
	// First, send the HTTP Common Header
	client_.println(HTTP_COMMON_HEADER);

	// Send all of it
	if (chunkSize == 0)
		client_.print(buffer_);

	// Send chunk by chunk #####################################

	// Max iterations
	uint8_t max = (int) (bufferIndex_ / chunkSize) + 1;

	// Send data
	for (uint8_t i = 0; i < max; i++) {
		char bufferAux[chunkSize + 1];
		memcpy(bufferAux, buffer_ + i * chunkSize, chunkSize);
		bufferAux[chunkSize] = '\0';

		// DLOGChar(bufferAux);
		client_.print(bufferAux);

		// Wait for client_ to get data
		delay(delayTime);
	}
}

void RestServer::check() {
	char route[ROUTES_LENGHT] = { 0 };
	bool routePrepare = false;
	bool routeCatchFinished = false;
	uint8_t r = 0;

	char query[QUERY_LENGTH] = { 0 };
	bool queryPrepare = false;
	bool queryCatchFinished = false;
	uint8_t q = 0;

	char method[METHODS_LENGTH] = { 0 };
	bool methodCatchFinished = false;
	uint8_t m = 0;

	char body[BODY_LENGTH] = { 0 };
	uint8_t b = 0;

	bool startIgnoreFlag = true;
	bool currentLineIsBlank = true;
	char c;
	while (client_.connected() && client_.available()) {
		c = client_.read();
		if (c == '\n' && currentLineIsBlank) {
			// Here is where the parameters of other HTTP Methods will be.
			while (client_.available() && client_.connected()) {
				int byte = client_.read();
				if (b < BODY_LENGTH) {
					body[b++] = byte;
//					Serial.println((char)byte);
				} else {
					if (startIgnoreFlag) {
						Serial.print("Ignoring: ");
						startIgnoreFlag = false;
					}
					Serial.print((char) byte);
				}
			}
			Serial.println();

			break;
		}

		if (c == '\n')
			currentLineIsBlank = true; // you're starting a new line
		else if (c != '\r')
			currentLineIsBlank = false; // you've gotten a character on the current line
		// End end of line process //////////////////

		// Start route catch process ////////////////
		if (c == '/' && !routePrepare)
			routePrepare = true;

		if ((c == ' ' || c == '?') && routePrepare)
			routeCatchFinished = true;

		if (routePrepare && !routeCatchFinished)
			route[r++] = c;
		// End route catch process //////////////////

		// Start query catch process ////////////////
		if (c == ' ' && queryPrepare)
			queryCatchFinished = true;

		if (queryPrepare && !queryCatchFinished)
			query[q++] = c;

		if (c == '?' && !queryPrepare)
			queryPrepare = true;
		// End query catch process /////////////////

		// Start method catch process ///////////////
		if (c == ' ' && !methodCatchFinished)
			methodCatchFinished = true;

		if (!methodCatchFinished)
			method[m++] = c;
		// End method catch process /////////////////

	}

	for (int i = 0; i < routesIndex_; i++) {
		// Check if the routes names matches
		if (strncmp(route, routes_[i].name, sizeof(routes_[i].name)) != 0)
			continue;

		// Check if the HTTP METHOD matters for this route
		if (strncmp(routes_[i].method, "*", sizeof(routes_[i].method)) != 0) {
			// If it matters, check if the methods matches
			if (strncmp(method, routes_[i].method, sizeof(routes_[i].method))
					!= 0)
				continue;
		}

		// Route callback (function)
		routes_[i].callback(query, body, b);
	}

}
