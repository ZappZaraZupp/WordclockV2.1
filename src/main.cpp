#include "main.h"

///////////////////////////////////
void setup() {
	#ifdef DEBUGSERIAL
		Serial.begin(115200);
	#endif
	LOG("setup")

	myDisplay.init();
	#ifdef DEV_BOARD
		myDisplay.flipScreenVertically();
	#endif
	myDisplay.setFont(Monospaced_bold_16);
	myDisplay.clear();
	myDisplay.drawString(0, 0, "Wordclock\nV2.1");
	myDisplay.display();

	for(uint8_t i=0;i<8;i++) {
		pinMode(BtnPin[i], INPUT);
	}

	pinMode(LDRPin, INPUT);
	pinMode(PanelPixelPin, OUTPUT);

	PanelStrip.Begin();

	delay(1000);

	if (! myRtc.begin()) {
		myDisplay.clear();
		myDisplay.drawString(0, 0, "Couldn't attach RTC\nABORT");
		myDisplay.display();
		abort();
	}

	// default RTC.
	// NTP Time only if the correct buttons are pressed getNTPTime();

	//Test
	myDisplay.clear();
	myDisplay.drawString(0, 0, "LED Test");
	myDisplay.display();
	PanelStrip.ClearTo(RgbwColor(255,0,0,0));
	PanelStrip.Show();
	delay(500);
	PanelStrip.ClearTo(RgbwColor(0,255,0,0));
	PanelStrip.Show();
	delay(500);
	PanelStrip.ClearTo(RgbwColor(0,0,255,0));
	PanelStrip.Show();
	delay(500);
	PanelStrip.ClearTo(RgbwColor(0,0,0,255));
	PanelStrip.Show();
	delay(500);
	PanelStrip.ClearTo(RgbwColor(255,255,255,255));
	PanelStrip.Show();
	delay(500);
	PanelStrip.ClearTo(0);
	PanelStrip.Show();
	//EndTest

	getNTPTime();

}

////////////////////////////////////
void loop() {
	// get Time to display
	timeNow = myRtc.now();
	// Localtime
	if(summertime_DE(timeNow)) {
		timeNow = DateTime(timeNow.unixtime()+7200);
	}
	else {
		timeNow = DateTime(timeNow.unixtime()+3600);
	}
	
	// brightness [minBrightness;1[
	currentBright = minBrightness + (maxBrightness - minBrightness) * (4096.0 - analogRead(LDRPin)) / 4096.0;

	// Read Buttons (debounce)
	for(uint8_t i=0; i<8; i++) {
		if (digitalRead(BtnPin[i])) {
			//display.drawString(8*i, 40, String(i));
			if(debounceBtn[i] == 0) { // Button newly pressed - set timestamp
				debounceBtn[i]=millis();
			}
		}
		else {
			if(debounceBtn[i] != 0) {// button was released (debounceBtn holds a timestamp)
				BtnOn[i] = millis()-debounceBtn[i];
				debounceBtn[i] = 0;
			}
		}
	}
	// do something if a button was pressed long enough
	doButtons();

	//things only to be done every new second
	/* if(timeNow.second() != prevTimeNow.second()) {
		prevTimeNow = timeNow;
		setDisplay(timeNow);
		myDisplay.display();
	}
	*/
	setDisplay(timeNow);
	myDisplay.display();

	setPixels();

	if(PanelAnimation.IsAnimating() && !(panelDirty || minutesDirty || PanelColorModeDirty || MinutesColorModeDirty)) { 
		PanelAnimation.UpdateAnimations();
		PanelStrip.Show();
	}
	else {
		SetupMinutesAnimation();
		SetupPanelAnimation();
	}
}

////////////////////////////////////
void getNTPTime() {
	myDisplay.clear();
	myDisplay.drawString(0, 0, "Connecting to \nWIFI");
	myDisplay.display();
	WiFi.begin(ssid, password);
  	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		LOG(".");
	}
	configTime(0, 0, "192.168.42.99");
	struct tm ntptime;
	if(!getLocalTime(&ntptime)) {
		myDisplay.clear();
		myDisplay.drawString(0, 0, "Couldn't get\nNTP time");
		myDisplay.display();
		delay(1000);
  	}
	else {
		#ifdef DEBUGSERIAL
			Serial.println(&ntptime, "%A, %B %d %Y %H:%M:%S");
		#endif
		myRtc.adjust(DateTime(ntptime.tm_year+1900,ntptime.tm_mon+1,ntptime.tm_mday,ntptime.tm_hour,ntptime.tm_min,ntptime.tm_sec));
		myDisplay.clear();
		myDisplay.drawString(0, 0, "Time set\nfrom NTP");
		myDisplay.display();
		delay(1000);
	}
	WiFi.disconnect(true);
  	WiFi.mode(WIFI_OFF);
}

////////////////////////////////////
// return value: returns true during Daylight Saving Time, false otherwise
boolean summertime_DE(DateTime t) {
	uint8_t tzHours = 1; // DE = UTC+1
	uint16_t year = t.year();
	uint8_t month = t.month();
	uint8_t day = t.day();
	uint8_t hour = t.hour();
	if (month<3 || month>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
	if (month>3 && month<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
	if ((month == 3 && (hour + 24 * day)>=(1 + tzHours + 24*(31 - (5 * year /4 + 4) % 7))) || (month == 10 && (hour + 24 * day)<(1 + tzHours + 24*(31 - (5 * year /4 + 1) % 7))))
    	return true;
	else
		return false;
}

////////////////////////////////////
// set display Text
void setDisplay(DateTime t) {
	// display message for showSeconds Time
	// message timer is set when a message is set
	char dt_string0[] = "YYYY-MM-DD\nhh:mm:ss";
	char dt_string1[] = "ss";
	char dt_string2[] = "hh:mm:ss\nDD.MM.YYYY";
		
	if ((displayMessage.message != "") || (displayMessage.message != oldDisplayMessage.message)) { // also check if message has changed during message display
		oldDisplayMessage.message = displayMessage.message;
		if (displayMessage.disptime == 0) { 
			displayMessage.disptime = timeNow.unixtime(); 
		}
		if((t.unixtime() - displayMessage.disptime) < displayMessage.showSeconds) {
			myDisplay.setFont(Monospaced_bold_16);
			myDisplay.clear();
			myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
			myDisplay.drawString(64, 5, displayMessage.message);
		}
		else {
			// reset message and normal display
			displayMessage.message = String("");
			oldDisplayMessage.message = String("");
			displayMessage.disptime = 0;
			setDisplay(t);
		}
	} 
	else {
		switch(DisplayMode){
		case 3: // empty display
			myDisplay.clear();
			break;
		case 2: // time and date
			myDisplay.clear();
			myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
			myDisplay.setFont(Monospaced_bold_16);
			myDisplay.drawString(64, 5, timeNow.toString(dt_string2));
			break;
		case 1: // only seconds
			myDisplay.clear();
			myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
			myDisplay.setFont(Monospaced_bold_50);
			myDisplay.drawString(64, 5, timeNow.toString(dt_string1));
			break;
		case 0:
		default:
			myDisplay.clear();
			myDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
			myDisplay.setFont(Monospaced_bold_16);
			myDisplay.drawString(64, 2, dayNames[timeNow.dayOfTheWeek()]);
			myDisplay.drawString(64, 22, timeNow.toString(dt_string0));
			myDisplay.drawRect(3,61,121,4);
			myDisplay.drawHorizontalLine(4,61+1,timeNow.second()*2);
			myDisplay.drawHorizontalLine(4,61+2,timeNow.second()*2);
			myDisplay.display();
		}
	}
}

void doButtons() {
	PanelColorModeDirty = false;
	MinutesColorModeDirty = false;

	// Buttons 0 and 1: Increase/decrease Panel Color Mode (decrease wins if both are pressed)
	if(BtnOn[0] > debounceMillis) { 
		BtnOn[0] = 0; // reset Timer
		PanelColorMode == 0 ? PanelColorMode=5: PanelColorMode -= 1;
		PanelColorModeDirty = true;
		displayMessage.message = String("PanelColorMode\n") + String(PanelColorModeText[PanelColorMode]);
	}
	else if(BtnOn[1] > debounceMillis) {
		BtnOn[1] = 0; // reset Timer
		PanelColorMode == 5 ? PanelColorMode=0: PanelColorMode += 1;
		PanelColorModeDirty = true;
		displayMessage.message = String("PanelColorMode\n") + String(PanelColorModeText[PanelColorMode]);
	}

	// Buttons 1 and 2: Increase/decrease Minutes Color Mode (decrease wins if both are pressed)
	if(BtnOn[2]  > debounceMillis) {  
		BtnOn[2] = 0; // reset Timer
		MinutesColorMode == 0 ? MinutesColorMode=5: MinutesColorMode -= 1;
		MinutesColorModeDirty = true;
		displayMessage.message = String("MinutesColorMode\n") + String(MinutesColorModeText[MinutesColorMode]);
	}
	else if(BtnOn[3]  > debounceMillis) {
		BtnOn[3] = 0; // reset Timer
		MinutesColorMode == 5 ? MinutesColorMode=0: MinutesColorMode += 1;
		MinutesColorModeDirty = true;
		displayMessage.message = String("MinutesColorMode\n") + String(MinutesColorModeText[MinutesColorMode]);
	}

	// Buttons 3 and 4: Increase/decrease displayMode (decrease wins if both are pressed)
	if(BtnOn[4]  > debounceMillis) {  
		BtnOn[4] = 0; // reset Timer
		DisplayMode == 0 ? DisplayMode=3: DisplayMode -= 1;
		displayMessage.message = String("DisplayMode\n") + String(DisplayModeText[DisplayMode]);
	}
	else if(BtnOn[5]  > debounceMillis) {
		BtnOn[5] = 0; // reset Timer
		DisplayMode == 3 ? DisplayMode=0: DisplayMode += 1;
		displayMessage.message = String("DisplayMode\n") + String(DisplayModeText[DisplayMode]);
	}

	// Button 6 - switch between light all minute LEDs or only the one showing the number
	if(BtnOn[6]  > debounceMillis) {
		BtnOn[6] = 0; // reset Timer
		MinutesColorModeDirty = true;
		if(MinutesAnimMode != 0) {
			MinutesAnimMode = 0;
		}
		else {
			MinutesAnimMode = 1;
		}
	}

	// Button 7 - longpress: connect wifi, set time
	if(BtnOn[7]  > debounceMillisLong) {
		BtnOn[7] = 0; // reset Timer
		getNTPTime();
	}
}


/////////////////////////////////////
// set all Pixels
void setPixels() {
	getTimeText();
	getMinutesText();
}

/////////////////////////////////////
// hours text
void HourText(uint8_t h) {
	if (h < 100) { // >=100 special cases
		h %= 12;
	}
	switch (h) {
	case 0:
	case 12:
		ZWOLF;
		break;
	case 1:
		EINS;
		break;
	case 2:
		ZWEI;
		break;
	case 3:
		DREI_2;
		break;
	case 4:
		VIER_2;
		break;
	case 5:
		FUNF_2;
		break;
	case 6:
		SECHS;
		break;
	case 7:
		SIEBEN;
		break;
	case 8:
		ACHT;
		break;
	case 9:
		NEUN;
		break;
	case 10:
		ZEHN_2;
		break;
	case 11:
		ELF;
		break;
		// special cases
	case 100:
		EIN;
		break;
	}
}

/////////////////////////////////////
// get time and set panelMask
void getTimeText() {
	// panelMask-lines reset
	// store old value
	for (uint8_t i = 0; i < 10; i++) {
		oldPanelMask[i] = panelMask[i];
		panelMask[i] = 0;
	}

	// ES IST
	ES;
	IST_1;

	if (timeNow.minute() < 5) {
		HourText((timeNow.hour() % 12) == 1 ? 100 : timeNow.hour()); // "Es ist Ein Uhr" (nicht: "Es ist Eins Uhr" ;-)
		UHR;
	}
	else if (timeNow.minute() < 10) {
		//00:05	Es ist fünf[0] nach xx
		FUNF_1;
		NACH_1;
		HourText(timeNow.hour());
	}
	else if (timeNow.minute() < 15) {
		//00:10	Es ist zehn[1] nach xx
		ZEHN_1;
		NACH_1;
		HourText(timeNow.hour());
	}
	else if (timeNow.minute() < 20) {
		//00:15	Es ist viertel nach xx
		VIERTEL;
		NACH_1;
		HourText(timeNow.hour());
	}
	else if (timeNow.minute() < 25) {
		//00:20	Es ist zwanzig nach xx
		ZWANZIG;
		NACH_1;
		HourText(timeNow.hour());
	}
	else if (timeNow.minute() < 30) {
		//00:25	Es ist fünf[0] vor halb xx+1
		FUNF_1;
		VOR;
		HALB;
		HourText(timeNow.hour() + 1);
	}
	else if (timeNow.minute() < 35) {
		//00:30	Es ist halb xx+1
		HALB;
		HourText(timeNow.hour() + 1);
	}
	else if (timeNow.minute() < 40) {
		//00:35	Es ist fünf[0] nach halb xx+1
		FUNF_1;
		NACH_1;
		HALB;
		HourText(timeNow.hour() + 1);
	}
	else if (timeNow.minute() < 45) {
		//00:40	Es ist zwanzig vor xx+1
		ZWANZIG;
		VOR;
		HourText(timeNow.hour() + 1);
	}
	else if (timeNow.minute() < 50) {
		//00:45	Es ist viertel vor xx+1
		//		Es ist dreiviertel xx+1
		//if ((int)random(2) == 0) {
			VIERTEL;
			VOR;
			HourText(timeNow.hour() + 1);
		//}
		//else {
		//	DREIVIERTEL;
		//	HourText(timeNow.hour() + 1);
		//}
	}
	else if (timeNow.minute() < 55) {
		//00:50	Es ist zehn[1] vor xx+1
		ZEHN_1;
		VOR;
		HourText(timeNow.hour() + 1);
	}
	else {
		//00:55	Es ist fünf[0] vor xx+1
		FUNF_1;
		VOR;
		HourText(timeNow.hour() + 1);
	}
	// check if something has changed
	panelDirty = false;
	for (uint8_t i = 0; i < 10; i++) {
		if(oldPanelMask[i] != panelMask[i]) {panelDirty = true;}
	}
}

/////////////////////////////////////
// get Minutes
void getMinutesText() {
	oldMinutesMask = minutesMask;
	minutesMask = 0;
	switch (timeNow.minute() % 5) {
	case 1:
		if(MinutesAnimMode==0) {
			minutesMask = 0b10000000;
		}
		else {
			minutesMask = 0b10000000;
		}
		break;
	case 2:
		if(MinutesAnimMode==0) {
			minutesMask = 0b11000000;
		}
		else {
			minutesMask = 0b01000000;
		}
		break;
	case 3:
		if(MinutesAnimMode==0) {
			minutesMask = 0b11100000;
		}
		else {
			minutesMask = 0b00100000;
		}
		break;
	case 4:
		if(MinutesAnimMode==0) {
			minutesMask = 0b11110000;
		}
		else {
			minutesMask = 0b00010000;
		}
		break;
	}
	// check if something has changed
	minutesDirty = false;
	if(oldMinutesMask != minutesMask) {minutesDirty = true;}
}

/////////////////////////////////////
// 
void SetupPanelAnimation() { // initialize AnimationState from getPixelColor and panelMask
	uint8_t x = 0;
	RgbwColor newColor;
	for (int i = 0; i < PanelHeight; i++) { // row
		//LOG(String(i)+" : "+String(panelMask[i]));
		for (int j = 0; j < PanelWidth; j++) { // column
			x = bitRead(panelMask[i], 15 - j);
			if (x == 1) {
				StripState[topo.Map(j, i)].StartingColor = PanelStrip.GetPixelColor(topo.Map(j, i));
				switch(PanelColorMode){
					case 5: //Colorwheel per minute
						newColor = colorWheel(60, timeNow.second() ,currentBright,0);
						break;
					case 4: //Red
						newColor = RgbwColor(255*currentBright, 0*currentBright, 0*currentBright, 0*currentBright);
					break;
					case 3: //Green
						newColor = RgbwColor(0*currentBright, 255*currentBright, 0*currentBright, 0*currentBright);
					break;
					case 2: //Blue
						newColor = RgbwColor(0*currentBright, 0*currentBright, 255*currentBright, 0*currentBright);
					break;
					case 1: //White
						newColor = RgbwColor(0*currentBright, 0*currentBright, 0*currentBright, 255*currentBright);
					break;
					case 0: //ColorWheel over Pixel circle per hour
					default:
						newColor =  colorWheel(PanelPixelCount,topo.Map(j, i),currentBright,(uint16_t)(PanelPixelCount*timeNow.minute()/60.0));
				}
				StripState[topo.Map(j, i)].EndingColor = newColor;
			}
			else {
				StripState[topo.Map(j, i)].StartingColor = PanelStrip.GetPixelColor(topo.Map(j, i));
				StripState[topo.Map(j, i)].EndingColor = RgbwColor(0, 0, 0, 0);
			}
			// Start anim only, if pixel has changed
			if(StripState[topo.Map(j, i)].StartingColor != StripState[topo.Map(j, i)].EndingColor) {
				PanelAnimation.StartAnimation(topo.Map(j, i),50,FadeAnim);
			}
		}
	}
}

void SetupMinutesAnimation() { // initialize AnimationState from getPixelColor and minutesMask
	uint8_t x = 0;
	for (uint8_t j = 0; j <= 3; j++) {
		x = bitRead(minutesMask, 7-j);
		if (x == 1) {
            StripState[MinutesPixelStart+j].StartingColor = PanelStrip.GetPixelColor(MinutesPixelStart+j);
			switch(MinutesColorMode){
				case 5: //Colorwheel per minute
					StripState[MinutesPixelStart+j].EndingColor = colorWheel(60, timeNow.second() ,currentBright,0);
				break;
				case 4: //Red
					StripState[MinutesPixelStart+j].EndingColor = RgbwColor(255*currentBright, 0*currentBright, 0*currentBright, 0*currentBright);
				break;
				case 3: //Green
					StripState[MinutesPixelStart+j].EndingColor = RgbwColor(0*currentBright, 255*currentBright, 0*currentBright, 0*currentBright);
				break;
				case 2: //Blue
					StripState[MinutesPixelStart+j].EndingColor = RgbwColor(0*currentBright, 0*currentBright, 255*currentBright, 0*currentBright);
				break;
				case 1: //White
					StripState[MinutesPixelStart+j].EndingColor = RgbwColor(0*currentBright, 0*currentBright, 0*currentBright, 255*currentBright);
				break;
				case 0: //ColorWheel over Pixel
				default:
					StripState[MinutesPixelStart+j].EndingColor = colorWheel(MinutesPixelCount,j,currentBright,(uint16_t)(MinutesPixelCount*timeNow.minute()/60.0));
			}
		}
		else {
            StripState[MinutesPixelStart+j].StartingColor = PanelStrip.GetPixelColor(MinutesPixelStart+j);
			StripState[MinutesPixelStart+j].EndingColor = RgbwColor(0, 0, 0, 0);
		}
		// Start anim only, if pixel has changed
        if(StripState[MinutesPixelStart+j].StartingColor != StripState[MinutesPixelStart+j].EndingColor) {
            PanelAnimation.StartAnimation(MinutesPixelStart+j,50,FadeAnim);
        }
	}
}

void FadeAnim(AnimationParam param) {
    //simple linear blend
    RgbwColor updatedColor = RgbwColor::LinearBlend(
        StripState[param.index].StartingColor,
        StripState[param.index].EndingColor,
        param.progress);
    // apply the color to the strip
    PanelStrip.SetPixelColor(param.index, updatedColor);
}

/////////////////////////////////////
// RGB Collor wheel
RgbwColor colorWheel(uint16_t wheelsteps, uint16_t curstep, float currentBright, uint16_t offset) {

	float p = wheelsteps / 3.0;
	float s = 255.0 / p; // stepsize
	curstep = (uint16_t)(curstep + offset * s) % wheelsteps;

	// 255,0,0 --> 0,255,0
	if (curstep < p) {
		return RgbwColor((255 - curstep * s) * currentBright, curstep * s * currentBright, 0, 0);
	}
	// 0,255,0 --> 0,0,255
	if (curstep < 2 * p) {
		curstep -= p;
		return RgbwColor(0, (255 - curstep * s) * currentBright, curstep * s * currentBright, 0);
	}
	// 0,0,255 --> 255,0,0
	curstep -= 2 * p;
	return RgbwColor(curstep * s * currentBright, 0, (255 - curstep * s) * currentBright, 0);
}

/////////////////////////////////////
// Tests
void PixelColorWheel(uint8_t from, uint8_t to, uint8_t wait) {
	// todo: check borders
	for (uint8_t i = from;  i < to; i++) {
		PanelStrip.SetPixelColor(i, colorWheel(to-from, i-from, 1.0,0));
		PanelStrip.Show();
		delay(wait);
	}
}
