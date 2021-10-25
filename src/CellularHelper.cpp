//#include "Particle.h"

#include "CellularHelper.h"

// This check is here so it can be a library dependency for a library that's compiled for both
// cellular and Wi-Fi.
#if Wiring_Cellular

// When using system firmware 0.6.0RC1 or later, use the actual Log object, but for older
// versions just add a dummy class that does nothing so the code will compile.
#ifndef SYSTEM_VERSION_060RC1
class LogClass {
public:
	inline void info(const char *fmt, ...) {
		// This is used in the unit test, not running on an actual device
		/*
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
		printf("\n");
		*/
	}
};
static LogClass Log;
#endif

CellularHelperClass CellularHelper;

void CellularHelperCommonResponse::logCellularDebug(int type, const char *buf, int len) const {
	String typeStr;
	switch(type) {
	case TYPE_UNKNOWN:
		typeStr = "TYPE_UNKNOWN";
		break;

	case TYPE_OK:
		typeStr = "TYPE_OK";
		break;

	case TYPE_ERROR:
		typeStr = "TYPE_ERROR";
		break;

	case TYPE_RING:
		typeStr = "TYPE_ERROR";
		break;

	case TYPE_CONNECT:
		typeStr = "TYPE_CONNECT";
		break;

	case TYPE_NOCARRIER:
		typeStr = "TYPE_NOCARRIER";
		break;

	case TYPE_NODIALTONE:
		typeStr = "TYPE_NODIALTONE";
		break;

	case TYPE_BUSY:
		typeStr = "TYPE_BUSY";
		break;

	case TYPE_NOANSWER:
		typeStr = "TYPE_NOANSWER";
		break;

	case TYPE_PROMPT:
		typeStr = "TYPE_PROMPT";
		break;

	case TYPE_PLUS:
		typeStr = "TYPE_PLUS";
		break;

	case TYPE_TEXT:
		typeStr = "TYPE_PLUS";
		break;

	case TYPE_ABORTED:
		typeStr = "TYPE_ABORTED";
		break;

	default:
		typeStr = String::format("type=0x%x", type);
		break;
	}

	Log.info("cellular response type=%s len=%d", typeStr.c_str(), len);
	String out;
	for(int ii = 0; ii < len; ii++) {
		if (buf[ii] == '\n') {
			out += "\\n";
			Log.info(out);
			out = "";
		}
		else
		if (buf[ii] == '\r') {
			out += "\\r";
		}
		else
		if (buf[ii] < ' ' || buf[ii] >= 127) {
			char hex[10];
			snprintf(hex, sizeof(hex), "0x%02x", buf[ii]);
			out.concat(hex);
		}
		else {
			out.concat(buf[ii]);
		}
	}
	if (out.length() > 0) {
		Log.info(out);
	}
}




int CellularHelperStringResponse::parse(int type, const char *buf, int len) {
	if (enableDebug) {
		logCellularDebug(type, buf, len);
	}
	if (type == TYPE_UNKNOWN) {
		CellularHelper.appendBufferToString(string, buf, len, true);
	}
	return WAIT;
}


int CellularHelperPlusStringResponse::parse(int type, const char *buf, int len) {
	if (enableDebug) {
		logCellularDebug(type, buf, len);
	}
	if (type == TYPE_PLUS) {
		// Copy to temporary string to make processing easier
		char *copy = (char *) malloc(len + 1);
		if (copy) {
			strncpy(copy, buf, len);
			copy[len] = 0;

			// We return the parts of the + response corresponding to the command we requested
			char searchFor[32];
			snprintf(searchFor, sizeof(searchFor), "\n+%s: ", command.c_str());

			//Log.info("searching for: +%s:", command.c_str());

			char *start = strstr(copy, searchFor);
			if (start) {
				start += strlen(searchFor);

				char *end = strchr(start, '\r');
				CellularHelper.appendBufferToString(string, start, end - start);
				//Log.info("found %s", string.c_str());
			}
			else {
				//Log.info("not found");
			}

			free(copy);
		}
	}
	return WAIT;
}

String CellularHelperPlusStringResponse::getDoubleQuotedPart(bool onlyFirst) const {
	String result;
	bool inQuoted = false;

	result.reserve(string.length());

	for(size_t ii = 0; ii < string.length(); ii++) {
		char ch = string.charAt(ii);
		if (ch == '"') {
			inQuoted = !inQuoted;
			if (!inQuoted && onlyFirst) {
				break;
			}
		}
		else {
			if (inQuoted) {
				result.concat(ch);
			}
		}
	}

	return result;
}


void CellularHelperRSSIQualResponse::postProcess() {
	if (sscanf(string.c_str(), "%d,%d", &rssi, &qual) == 2) {

		// The range is the following:
		// 0: -113 dBm or less
		// 1: -111 dBm
		// 2..30: from -109 to -53 dBm with 2 dBm steps
		// 31: -51 dBm or greater
		// 99: not known or not detectable or currently not available
		if (rssi < 99) {
			rssi = -113 + (rssi * 2);
		}
		else {
			rssi = 0;
		}

		resp = RESP_OK;
	}
	else {
		// Failed to parse result
		resp = RESP_ERROR;
	}
}

/*
CellularHelperEnvironmentResponse::CellularHelperEnvironmentResponse(CellularHelperEnvironmentCellData *neighbors, size_t numNeighbors) :
	neighbors(neighbors), numNeighbors(numNeighbors) {


}
*/
int CellularHelperEnvironmentResponse::parse(int type, const char *buf, int len) {
	if (enableDebug) {
		logCellularDebug(type, buf, len);
	}

	if (type == TYPE_UNKNOWN || type == TYPE_PLUS) {
		// We get this for AT+CGED=5
		// Copy to temporary string to make processing easier
		char *copy = (char *) malloc(len + 1);
		if (copy) {
			strncpy(copy, buf, len);
			copy[len] = 0;

			// This is used for skipping over the +CGED: part of the response
			char searchFor[32];
			size_t searchForLen = snprintf(searchFor, sizeof(searchFor), "+%s: ", command.c_str());

			char *endStr;

			char *line = strtok_r(copy, "\r\n", &endStr);
			while(line) {
				if (line[0]) {
					// Not an empty line

					if (type == TYPE_PLUS && strncmp(line, searchFor, searchForLen) == 0) {
						line += searchForLen;
					}

					if (strncmp(line, "MCC:", 4) == 0) {
						// Line begins with MCC:
						// This happens for 2G and 3G
						if (curDataIndex < 0) {
							service.parse(line);
							curDataIndex++;
						}
						else
						if (neighbors && (size_t)curDataIndex < numNeighbors) {
							neighbors[curDataIndex++].parse(line);
						}
					}
					else
					if (strncmp(line, "RAT:", 4) == 0) {
						// Line begins with RAT:
						// This happens for 3G in the + response so you know whether
						// the response is for a 2G or 3G tower
						service.parse(line);
					}
				}
				line = strtok_r(NULL, "\r\n", &endStr);
			}

			free(copy);
		}
	}
	return WAIT;
}

void CellularHelperEnvironmentCellData::parse(const char *str) {
	char *mutableCopy = strdup(str);

	char *endStr;

	char *pair = strtok_r(mutableCopy, ",", &endStr);
	while(pair) {
		// Remove leading spaces caused by ", " combination
		while(*pair == ' ') {
			pair++;
		}

		char *colon = strchr(pair, ':');
		if (colon != NULL) {
			*colon = 0;
			const char *key = pair;
			const char *value = ++colon;

			addKeyValue(key, value);
		}

		pair = strtok_r(NULL, ",", &endStr);
	}


	free(mutableCopy);
}

bool CellularHelperEnvironmentCellData::isValid(bool ignoreCI) const {

	if (mcc > 999) {
		return false;
	}

	if (!ignoreCI) {
		if (isUMTS) {
			if (ci >= 0xfffffff) {
				return false;
			}
		}
		else {
			if (ci >= 0xffff) {
				return false;
			}
		}
	}
	return true;
}


void CellularHelperEnvironmentCellData::addKeyValue(const char *key, const char *value) {
	char ucCopy[16];
	if (strlen(key) > (sizeof(ucCopy) - 1)) {
		Log.info("key too long key=%s value=%s", key, value);
		return;
	}
	size_t ii = 0;
	for(; key[ii]; ii++) {
		ucCopy[ii] = toupper(key[ii]);
	}
	ucCopy[ii] = 0;

	if (strcmp(ucCopy, "RAT") == 0) {
		isUMTS = (strstr(value, "UMTS") != NULL);
	}
	else
	if (strcmp(ucCopy, "MCC") == 0) {
		mcc = atoi(value);
	}
	else
	if (strcmp(ucCopy, "MNC") == 0) {
		mnc = atoi(value);
	}
	else
	if (strcmp(ucCopy, "LAC") == 0) {
		lac = (int) strtol(value, NULL, 16); // hex
	}
	else
	if (strcmp(ucCopy, "CI") == 0) {
		ci = (int) strtol(value, NULL, 16); // hex
	}
	else
	if (strcmp(ucCopy, "BSIC") == 0) {
		bsic = (int) strtol(value, NULL, 16); // hex
	}
	else
	if (strcmp(ucCopy, "ARFCN") == 0) { // Usually "Arfcn"
		// Documentation says this is hex, but this does not appear to be the case!
		// arfcn = (int) strtol(value, NULL, 16); // hex
		arfcn = atoi(value);
	}
	else
	if (strcmp(ucCopy, "ARFCN_DED") == 0 || strcmp(ucCopy, "RXLEVSUB") == 0 || strcmp(ucCopy, "T_ADV") == 0) {
		// Ignored 2G fields: Arfcn_ded, RxLevSub, t_adv
	}
	else
	if (strcmp(ucCopy, "RXLEV") == 0) { // Sometimes RxLev
		rxlev = (int) strtol(value, NULL, 16); // hex
	}
	else
	if (strcmp(ucCopy, "DLF") == 0) {
		dlf = atoi(value);
	}
	else
	if (strcmp(ucCopy, "ULF") == 0) {
		ulf = atoi(value);

		// For AT+COPS=5, we don't get a RAT, but if ULF is present it's 3G
		isUMTS = true;
	}
	else
	if (strcmp(ucCopy, "RSCP LEV") == 0) {
		rscpLev = atoi(value);
	}
	else
	if (strcmp(ucCopy, "RAC") == 0 || strcmp(ucCopy, "SC") == 0 || strcmp(ucCopy, "ECN0 LEV") == 0) {
		// We get these with AT+COPS=5, but we don't need the values
	}
	else {
		Log.info("unknown key=%s value=%s", key, value);
	}

}

int CellularHelperEnvironmentCellData::getBand() const {
	int freq = 0;

	if (isUMTS) {
		// 3G radio
		if (ulf >= 0 && ulf <= 124) {
			freq = 900;
		}
		else
		if (ulf >= 128 && ulf <= 251) {
			freq = 850;
		}
		else
		if (ulf >= 512 && ulf <= 885) {
			freq = 1800;
		}
		else
		if (ulf >= 975 && ulf <= 1023) {
			freq = 900;
		}
		else
		if (ulf >= 1312 && ulf <= 1513) {
			freq = 1700;
		}
		else
		if (ulf >= 2712 && ulf <= 2863) {
			freq = 900;
		}
		else
		if (ulf >= 4132 && ulf <= 4233) {
			freq = 850;
		}
		else
		if ((ulf >= 4162 && ulf <= 4188) || (ulf >= 20312 && ulf <= 20363)) {
			freq = 800;
		}
		else
		if (ulf >= 9262 && ulf <= 9538) {
			freq = 1900;
		}
		else
		if (ulf >= 9612 && ulf <= 9888) {
			freq = 2100;
		}
	}
	else {
		// 2G, use arfcn
		if (arfcn >= 0 && arfcn <= 124) {
			freq = 900;
		}
		else
		if (arfcn >= 128 && arfcn <= 251) {
			freq = 850;
		}
		else
		if (arfcn >= 512 && arfcn <= 885) {
			freq = 1800;
		}
		else
		if (arfcn >= 975 && arfcn <= 1023) {
			freq = 900;
		}
	}
	return freq;
}

// Calculated
String CellularHelperEnvironmentCellData::getBandString() const {
	String band;

	int freq = getBand();

	if (isUMTS) {
		// 3G radio
		if ((ulf >= 0 && ulf <= 124) ||
			(ulf >= 128 && ulf <= 251)) {
			band = "GSM " + String(freq);
		}
		else
		if (ulf >= 512 && ulf <= 885) {
			band = "DCS 1800";
		}
		else
		if (ulf >= 975 && ulf <= 1023) {
			band = "ESGM 900";
		}
		else
		if (freq != 0) {
			band = "UMTS " + String(freq);
		}
		else {
			band = "3G unknown";
		}
	}
	else {
		// 2G, use arfcn

		if (arfcn >= 512 && arfcn <= 885) {
			band = "DCS 1800 or 1900";
		}
		else
		if (arfcn >= 975 && arfcn <= 1024) {
			band = "EGSM 900";
		}
		else
		if (freq != 0) {
			band = "GSM " + String(freq);
		}
		else {
			band = "2G unknown";
		}
	}


	return band;
}

int CellularHelperEnvironmentCellData::getRSSI() const {
	int rssi = 0;

	if (isUMTS) {
		// 3G radio

		if (rscpLev <= 96) {
			rssi = -121 + rscpLev;
		}
	}
	else {
		// 2G
		if (rxlev <= 96) {
			rssi = -121 + rxlev;
		}
	}
	return rssi;
}

int CellularHelperEnvironmentCellData::getBars() const {
	return CellularHelperClass::rssiToBars(getRSSI());
}

String CellularHelperEnvironmentCellData::toString() const {
	String common = String::format("mcc=%d, mnc=%d, lac=%x ci=%x band=%s rssi=%d",
			mcc, mnc, lac, ci, getBandString().c_str(), getRSSI());

	if (isUMTS) {
		return String::format("rat=UMTS %s dlf=%d ulf=%d", common.c_str(), dlf, ulf);
	}
	else {
		return String::format("rat=GSM %s bsic=%x arfcn=%d rxlev=%d", common.c_str(), bsic, arfcn, rxlev);
	}
}

void CellularHelperEnvironmentResponse::clear() {
	curDataIndex = -1;
}


void CellularHelperEnvironmentResponse::postProcess() {
}


void CellularHelperEnvironmentResponse::logResponse() const {
	Log.info("service %s", service.toString().c_str());
	if (neighbors) {
		for(size_t ii = 0; ii < numNeighbors; ii++) {
			if (neighbors[ii].isValid(true /* ignoreCI */)) {
				Log.info("neighbor %d %s", ii, neighbors[ii].toString().c_str());
			}
		}
	}
}

size_t CellularHelperEnvironmentResponse::getNumNeighbors() const {
	if (curDataIndex < 0) {
		return 0;
	}
	else {
		if (neighbors) {
			for(size_t ii = 0; ii < (size_t)curDataIndex; ii++) {
				if (!neighbors[ii].isValid()) {
					return ii;
				}
			}
		}
		return curDataIndex;
	}
}



// +UULOC: <date>,<time>,<lat>,<long>,<alt>,<uncertainty>

void CellularHelperLocationResponse::postProcess() {
	char *mutableCopy = strdup(string.c_str());
	if (mutableCopy) {
		char *part, *endStr;

		part = strtok_r(mutableCopy, ",", &endStr);
		if (part) {
			// part is date
			part = strtok_r(NULL, ",", &endStr);
			if (part) {
				// part is time
				part = strtok_r(NULL, ",", &endStr);
				if (part) {
					// part is lat
					lat = atof(part);

					part = strtok_r(NULL, ",", &endStr);
					if (part) {
						// part is lon
						lon = atof(part);

						part = strtok_r(NULL, ",", &endStr);
						if (part) {
							// part is alt
							alt = atoi(part);

							part = strtok_r(NULL, ",", &endStr);
							if (part) {
								// part is uncertainty
								uncertainty = atoi(part);
								valid = true;
								resp = RESP_OK;
							}
						}
					}
				}
			}
		}

		free(mutableCopy);
	}
}

String CellularHelperLocationResponse::toString() const {
	if (valid) {
		return String::format("lat=%f lon=%f alt=%d uncertainty=%d", lat, lon, alt, uncertainty);
	}
	else {
		return "valid=false";
	}
}

void CellularHelperCREGResponse::postProcess() {
	// "\r\n+CREG: 2,1,\"FFFE\",\"C45C010\",8\r\n"
	int n;

	if (sscanf(string.c_str(), "%d,%d,\"%x\",\"%x\",%d", &n, &stat, &lac, &ci, &rat) == 5) {
		// SARA-R4 does include the n (5 parameters)
		valid = true;
	}
	else
	if (sscanf(string.c_str(), "%d,\"%x\",\"%x\",%d", &stat, &lac, &ci, &rat) == 4) {
		// SARA-U and SARA-G don't include the n (4 parameters)
		valid = true;
	}

}

String CellularHelperCREGResponse::toString() const {
	if (valid) {
		return String::format("stat=%d lac=0x%x ci=0x%x rat=%d", stat, lac, ci, rat);
	}
	else {
		return "valid=false";
	}
}

void CellularHelperCEREGResponse::postProcess() {
	// "\r\n+CEREG: 2,1,\"FFFE\",\"C45C010\",8\r\n"
	// "\r\n+CEREG: 2,1,"3a9b","0000c33d",7\r\n"
	//int n;

	if (sscanf(string.c_str(), "%d,%d,\"%x\",\"%x\",%d", &n, &stat, &lac, &ci, &rat) == 5) {
		// SARA-R4 does include the n (5 parameters)
		valid = true;
	}
	else
	if (sscanf(string.c_str(), "%d,\"%x\",\"%x\",%d", &stat, &lac, &ci, &rat) == 4) {
		// SARA-U and SARA-G don't include the n (4 parameters)
		valid = true;
	}
	else
	if (sscanf(string.c_str(), "%d,%d", &n, &stat) == 2) {
		// in case n=0
		valid = true;
	}
}

String CellularHelperCEREGResponse::toString() const {
	if (valid) {
		return String::format("n=%d stat=%d lac=0x%x ci=0x%x rat=%d", n, stat, lac, ci, rat);
	}
	else {
		return "valid=false";
	}
}

void CellularHelperPsmStatusResponse::postProcess() 
{
	if (string.endsWith("0"))
	{
		valid = true;
		stat = 0;
	}
	else
	if (string.endsWith("1"))
	{
		valid = true;
		stat = 1;
	}
	else{
		valid = false;
		stat = 0;
	}

	string = "";
}

String CellularHelperPsmStatusResponse::toString() const {
	if (valid) {
		return String::format("psm status=%d", stat);
	}
	else {
		return "valid=false";
	}
}

String CellularHelperClass::getManufacturer() const {
	CellularHelperStringResponse resp;

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CGMI\r\n");

	return resp.string;
}

String CellularHelperClass::getModel() const {
	CellularHelperStringResponse resp;

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CGMM\r\n");

	return resp.string;
}

String CellularHelperClass::getOrderingCode() const {
	CellularHelperStringResponse resp;

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "ATI0\r\n");

	return resp.string;
}

String CellularHelperClass::getFirmwareVersion() const {
	CellularHelperStringResponse resp;

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CGMR\r\n");

	return resp.string;
}

String CellularHelperClass::getIMEI() const {
	CellularHelperStringResponse resp;

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CGSN\r\n");

	return resp.string;
}

String CellularHelperClass::getIMSI() const {
	CellularHelperStringResponse resp;

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CGMI\r\n");

	return resp.string;
}

String CellularHelperClass::getICCID() const {
	CellularHelperPlusStringResponse resp;
	resp.command = "CCID";

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CCID\r\n");

	return resp.string;
}

bool CellularHelperClass::isLTE() const {
	String model;

	model = getModel();

	if(model.indexOf("SARA-R4") >= 0)
		return true;
	else
		return false;
}


String CellularHelperClass::getOperatorName(int operatorNameType) const {
	String result;

	// The default is OPERATOR_NAME_LONG_EONS (9).
	// If the EONS name is not available, then the other things tried in order are:
	// NITZ, CPHS, ROM
	// So basically, something will be returned

	CellularHelperPlusStringResponse resp;
	resp.command = "UDOPN";

	int respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+UDOPN=%d\r\n", operatorNameType);

	if (respCode == RESP_OK) {
		result = resp.getDoubleQuotedPart();
	}

	return result;
}

/**
 * Get the RSSI and qual values for the receiving cell site.
 *
 * The qual value is always 99 for me on the G350 (2G).
 */
CellularHelperRSSIQualResponse CellularHelperClass::getRSSIQual() const {
	CellularHelperRSSIQualResponse resp;
	resp.command = "CSQ";

	resp.resp = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CSQ\r\n");

	if (resp.resp == RESP_OK) {
		resp.postProcess();
	}

	return resp;
}
/*
bool CellularHelperClass::selectOperator(const char *mccMnc) const {
	CellularHelperStringResponse resp;

	int respCode;

	if (mccMnc == NULL) {
		// Reset back to automatic mode
		respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=0\r\n");
		return (respCode == RESP_OK);
	}

	String curMccMnc = CellularHelper.getOperatorName(0); // 0 = MCC/MNC
	if (strcmp(mccMnc, curMccMnc.c_str()) == 0) {
		// Operator already selected; nothing to do
		Log.info("operator already %s", mccMnc);
		return true;
	}

	if (curMccMnc.length() != 0) {
		// Disconnect from the current operator if there is an operator set.
		// On cold boot there won't be a name set and the string will be empty
		respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=2\r\n");
	}

	// Connect
	respCode = Cellular.command(responseCallback, (void *)&resp, 60000, "AT+COPS=4,2,\"%s\"\r\n", mccMnc);

	return (respCode == RESP_OK);
}
*/
bool CellularHelperClass::setRAT(int primary, int secondary) const {
	CellularHelperStringResponse resp;

	int respCode;

	// deregister first
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=2\r\n");

	// set RAT mode
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+URAT=%d,%d\r\n", primary, secondary);

	// reregister
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=0\r\n");

	return (respCode == RESP_OK);
}

bool CellularHelperClass::setRAT(int primary) const {
	CellularHelperStringResponse resp;

	int respCode;

	// deregister first
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=2\r\n");

	// set RAT mode
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+URAT=%d\r\n", primary);

	// reregister
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=0\r\n");

	return (respCode == RESP_OK);
}

String CellularHelperClass::getRAT() const {
	CellularHelperPlusStringResponse resp;
	resp.command = "URAT";

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+URAT?\r\n");

	return resp.string;
}

bool CellularHelperClass::setMNO(int profile) const {
	CellularHelperStringResponse resp;

	int respCode;

	// deregister first
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=2\r\n");

	// set RAT mode
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+UMNOPROF=%d\r\n", profile);

	// reregister
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CFUN=15\r\n");

	// turn off echo again, seems to reset with changing the mno?
	//respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "ATE0\r\n");

	return (respCode == RESP_OK);
}

int CellularHelperClass::getMNO() const {
	CellularHelperPlusStringResponse resp;
	resp.command = "UMNOPROF";

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+UMNOPROF?\r\n");

	return atoi(resp.string);
}

String CellularHelperClass::getCOPS() const
{
	CellularHelperPlusStringResponse resp;
	resp.command = "COPS";

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS?\r\n");
	
	return resp.string;
}

String CellularHelperClass::getCEREG() const
{
	CellularHelperPlusStringResponse resp;
	resp.command = "CEREG";

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CEREG?\r\n");
	
	return resp.string;
}

String CellularHelperClass::getCREG() const
{
	CellularHelperPlusStringResponse resp;
	resp.command = "CREG";

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CREG?\r\n");
	
	return resp.string;
}

String CellularHelperClass::getLocalPSMSettings() const
{
	CellularHelperPlusStringResponse resp;
	resp.command = "CPSMS";

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CPSMS?\r\n");

	return resp.string;	
}

String CellularHelperClass::getNetworkPSMSettings() const
{
	CellularHelperPlusStringResponse resp;
	resp.command = "UCPSMS";

	Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+UCPSMS?\r\n");
	
	return resp.string;	
}

bool CellularHelperClass::enterPSM() const
{
	int tempResp;
	CellularHelperPsmStatusResponse psm_resp;
	psm_resp.command = "UUPSMR";

	if (!isModemRegistered())
		return false;

	// set psm mode to network coordination mode only
	tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+UPSMVER=4\r\n");

	// reboot to enable psm mode
	tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+CFUN=15\r\n");

	// check psm mode
	tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+UPSMVER?\r\n");

	// enable psm
	// AT+CPSMS=1,,,"00100110","00000101"
	// 6 hours for TAU
	// 10 seconds for active time
	tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+CPSMS=1,,,\"00100110\",\"00000101\"\r\n");

	// enable radio connection status indication
	tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+CSCON=1\r\n");

	// enable psm indication
	tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+UPSMR=1\r\n");

	// reboot
	tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+CFUN=15\r\n");

	// look for when the modem goes into psm mode, by looking for the +UUPSMR = 1 message
	unsigned long startTime = millis();

	while(millis() - startTime < 30000) 
	{
		delay(100);

		// May have not received a response yet. Send an empty command so we can get responses
		Cellular.command(responseCallback, (void *)&psm_resp, 500, "");
		psm_resp.postProcess();

		if (psm_resp.valid && psm_resp.stat == 1)
			return(true);
	}

	return(false);
}

bool CellularHelperClass::disablePSM() const
{
	CellularHelperPlusStringResponse resp;
	resp.command = "CPSMS";

	int respCode;

	// turn off PSM functionality
	resp.resp = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CPSMS=0\r\n");

	// reboot
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CFUN=15\r\n");

	return (respCode == RESP_OK);	
}

bool CellularHelperClass::exitPSM() const
{
	// pull modem power_in line low for 150ms
	digitalWrite(PWR_UC, LOW);
	delay(150);
	digitalWrite(PWR_UC, HIGH);

	CellularHelperPsmStatusResponse resp;
	resp.command = "UUPSMR";

	// look for when the modem goes out of psm mode, by looking for the +UUPSMR = 0 message
	unsigned long startTime = millis();

	while(millis() - startTime < 10000) 
	{
		delay(100);

		// May have not received a response yet. Send an empty command so we can get responses
		Cellular.command(responseCallback, (void *)&resp, 500, "");
		resp.postProcess();

		if (resp.valid && resp.stat == 0)
			return(true);
	}

	return(false);
}

bool CellularHelperClass::isModemRegistered() const
{
	// check that the modem is registered
	CellularHelperCEREGResponse reg;
	getCEREG(reg);

	Log.info("CEREG %s", reg.toString());
	
	// check eps registration
	if (!(reg.stat == 1 || reg.stat == 5))
		return false;
	
	return true;
}
bool CellularHelperClass::configureLTE() const
{}
/*
bool CellularHelperClass::configureLTE() const
{
	CellularHelperStringResponse resp;
	int respCode;

	if (!isLTE())
		return(false);

	// deregister first
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=2\r\n");

	// set MNO mode
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+UMNOPROF=100\r\n");	// EU only!

	// reboot
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CFUN=15\r\n");

	// set RAT mode
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+URAT=7\r\n");

	// reboot
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CFUN=15\r\n");

	// set PSM mode
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CPSMS=0\r\n");

	// set EDRX mode
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CEDRXS=0\r\n");


	// reregister
	respCode = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+COPS=0\r\n");

	return (respCode == RESP_OK);	
}
*/
void CellularHelperClass::getEnvironment(int mode, CellularHelperEnvironmentResponse &resp) const {
	resp.command = "CGED";
	// resp.enableDebug = true;

	resp.resp = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CGED=%d\r\n", mode);
	if (resp.resp == RESP_OK) {
		resp.postProcess();
	}
}

CellularHelperLocationResponse CellularHelperClass::getLocation(unsigned long timeoutMs) const {
	CellularHelperLocationResponse resp;

	// Note: Command is ULOC, but the response is UULOC
	resp.command = "UULOC";
	// resp.enableDebug = true;

	// Initialize the mode
	resp.resp = Cellular.command(5000, "AT+ULOCCELL=0\r\n");
	if (resp.resp == RESP_OK) {
		unsigned long startTime = millis();

		resp.resp = Cellular.command(responseCallback, (void *)&resp, timeoutMs, "AT+ULOC=2,2,0,%d,5000\r\n", timeoutMs / 1000);

		// This command is weird because it returns an OK, and theoretically could return +UULOC response right away,
		// but usually does not.
		if (resp.resp == RESP_OK) {
			resp.postProcess();

			// In the case where we don't get an immediate response, we send empty commands to the
			// modem to pick up the late +UULOC response
			while(!resp.valid && millis() - startTime < timeoutMs) {
			//while(millis() - startTime < timeoutMs) {
				// Allow for some cloud processing before checking again
				delay(10);

				// Have not received a response yet. Send an empty command so we can get responses that
				// come afte the OK due to the weird structure of this command
				Cellular.command(responseCallback, (void *)&resp, 500, "");
				resp.postProcess();
			}
		}
	}

	return resp;
}

void CellularHelperClass::getCREG(CellularHelperCREGResponse &resp) const {
	int tempResp;

	tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+CREG=2\r\n");
	if (tempResp == RESP_OK) {
		resp.command = "CREG";
		resp.resp = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CREG?\r\n");
		if (resp.resp == RESP_OK) {
			resp.postProcess();

			// Set back to default
			tempResp = Cellular.command(DEFAULT_TIMEOUT, "AT+CREG=0\r\n");
		}
	}
}

void CellularHelperClass::getCEREG(CellularHelperCEREGResponse &resp) const {
	int tempResp;

	resp.command = "CEREG";
	resp.resp = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+CEREG?\r\n");
	if (resp.resp == RESP_OK) {
		resp.postProcess();
	}
}

bool CellularHelperClass::ping(const char *addr) const {
	CellularHelperStringResponse resp;

	resp.resp = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+UPING=\"%s\"\r\n", addr);

	return resp.resp == RESP_OK;
}

IPAddress CellularHelperClass::dnsLookup(const char *hostname) const {
	IPAddress result;

	CellularHelperPlusStringResponse resp;
	resp.command = "UDNSRN";

	resp.resp = Cellular.command(responseCallback, (void *)&resp, DEFAULT_TIMEOUT, "AT+UDNSRN=0,\"%s\"\r\n", hostname);
	if (resp.resp == RESP_OK) {
		String quotedPart = resp.getDoubleQuotedPart();
		int addr[4];
		if (sscanf(quotedPart.c_str(), "%u.%u.%u.%u", &addr[0], &addr[1], &addr[2], &addr[3]) == 4) {
			result = IPAddress(addr[0], addr[1], addr[2], addr[3]);
		}
	}

	return result;
}




// There isn't an overload of String that takes a buffer and length, but that's what comes back from
// the Cellular.command callback, so that's why this method exists.
void CellularHelperClass::appendBufferToString(String &str, const char *buf, int len, bool noEOL) const {
	str.reserve(str.length() + (size_t)len + 1);
	for(int ii = 0; ii < len; ii++) {
		if (!noEOL || (buf[ii] != '\r' && buf[ii] != '\n')) {
			str.concat(buf[ii]);
		}
	}
}

// static
int CellularHelperClass::rssiToBars(int rssi) {
	int bars = 0;

	if (rssi < 0) {
		if (rssi >= -57)      bars = 5;
		else if (rssi > -68)  bars = 4;
		else if (rssi > -80)  bars = 3;
		else if (rssi > -92)  bars = 2;
		else if (rssi > -104) bars = 1;
	}
	return bars;
}

// static
int CellularHelperClass::responseCallback(int type, const char* buf, int len, void *param) {
	CellularHelperCommonResponse *presp = (CellularHelperCommonResponse *)param;

	return presp->parse(type, buf, len);
}

#endif /* Wiring_Cellular */
