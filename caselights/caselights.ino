  //libraries
#include <Adafruit_NeoPixel.h>


	//software definitions
#define lednum 8			// number of leds to be set in chain
#define hddweight 5			//persentage weight for hdd activity indication

	//hardware definitions
#define ledpin 3			// serial line to addressable leds
#define resetbutton 4			// connected to reset button on front panel
#define powerbutton 5
#define sidepanel 8			// switch detecting presence of side panel on chassis
#define hddled 9			// connected to hdd activity indicator on front panel (pwm required)
#define resetpin 10			// mb reset line
#define powerpin 11

	//process definitions
#define mode_usr_a 0		//	common mode serial set a
#define mode_usr_b 1		//	common mode serial set b
#define mode_usr_c 2		//	common mode serial set c
#define mode_usr_d 3		//	common mode serial set d
#define mode_slow_shift 4	//	shifts colours from led to led
#define mode_glow 5			//	rotates hue space
#define mode_glow_mod 6		//	rotates hue space and modulates brightness
#define mode_contrast 7		//	some leds off, some on
#define mode_flood 8		//	all leds on
#define mode_nolights 9		//	all leds off
#define mode_usr_def 10		//	common mode serial set / button set
#define mode_count 10		//			NOT A MODE! number of regular togglable modes ^
#define mode_power_on 11	//	fade into mode_usr_a
#define mode_power_off 13	//	fade from current mode to 0
#define mode_hdd_active 14	//	fade to green based on drive activity


  //globals
Adafruit_NeoPixel cases= Adafruit_NeoPixel(lednum, ledpin, NEO_GRB + NEO_KHZ800);
bool caseopen = false;    // is case open?
byte leds[lednum][4];     // array [module] [r,g,b, lum]
uint16_t presstime =0;    // time button was last pressed
uint16_t settime[4] = {0,0,0,0};      // time [r,g,b, lum] was last set
byte lastset = 0;
byte mode;
bool pressresolve = false;
byte usersetting [5][lednum][4];
bool serialset = false;



void setup()
{
	digitalWrite(resetpin,HIGH);
	Serial.begin(19200);
	Serial.println("BRIGHT BOY setup complete");
	Serial.println("Enter a character to customise colours");

}

bool conv()
{
	serialset = true;
	bool staying = true;
	char convo;
	byte input[4];
	byte chan=127;
	Serial.println("Select a channel to edit [a-e]");

	while(!Serial.available());
	convo = Serial.read();
	if (convo == 'a') chan = 0;
	else if (convo == 'b') chan = 1;
	else if (convo == 'c') chan = 2;
	else if (convo == 'd') chan = 3;
	else if (convo == 'e') chan = 4;
	else
	{
		serialset = false;
		return false;
	}

	while (staying)
	{
		Serial.println("Edit channel generic colour:");
		Serial.println("Input 4 channel 8 bit colour [r][g][b][l]");
		while (!Serial.available())
		{
			for(byte l=0; l<4; l++)
			{
				input[l] = Serial.read();
			}
		}

		for(byte i=0; i<lednum ;i++)
		{
			for(byte j=0; j<4; j++)
			{
				usersetting[chan][i][j] = input[j];
			}
		}

		bool staychan = true;
		while (staychan)
		{
			Serial.println("Edit a pixel? [y/n]");
			while(!Serial.available());
			convo = Serial.read();
			if (convo == 'y')
			{
				Serial.print("Enter pixel number 0 - ");
				Serial.println((lednum - 1));

				while(!Serial.available());
				byte setpx = Serial.read();
				if(setpx < lednum)
				{
					Serial.println("Edit pixel colour:");
					Serial.println("Input 4 channel 8 bit colour [r] [g] [b] [l]");
					while (!Serial.available())
					{
						for(byte l=0; l<4; l++)
						{
							input[l] = Serial.read();
						}
					}
					for(byte j=0; j<4; j++)
					{
						usersetting[chan][setpx][j] = input[j];
					}
				}
				else
				{
					Serial.println("Invalid pixel value!");
					staychan = false;
				}

			}
			else staychan = false;
		}

		Serial.println("edit another channel? [y/n]");
		while (!Serial.available()) {}
		convo = Serial.read();
		if(convo == 'n') staying = false;
	}
	serialset = false;
	return true;
}



void reset()	//pull reset line low/high to reset mb
{
	digitalWrite(resetpin, LOW);
	delay(2);
	digitalWrite(resetpin, HIGH);
}


void advancemode()
{
	if(mode <= mode_count)
	{
		mode++;
	}
	else mode =0;
}


void setlevel()	//set leds to current colour after button released
{
	uint32_t level = millis() - presstime;
	level = (level << 8)/6550;
	for (int i = 0; i < lednum; i++)
	{
		usersetting[4][i][lastset] = (byte)level;
	}
	lastset++;
}


void buttonrls()                // 0-50   mode switch
{                               // 50-6600 level set
  if((millis()-presstime) < 50 && presstime != 0) // >6600 reset mb
  {
    advancemode();
  }
  else if((millis()-presstime) < 6600 && presstime != 0)
  {
    setlevel();
  }
  else reset();
  pressresolve = false;
}

void buttonprs()
{
	pressresolve = true;

	if (digitalRead(resetbutton))
	{

	}
	else if (presstime < millis() && presstime != 0)
	{
		buttonrls();
		presstime = 0;
	}
}


void setleds()	//to figure out & set state of leds
{
  if(digitalRead(resetbutton))
  {
    presstime = millis();
    buttonprs();
  }


  uint16_t set[3];
  for(byte i=0; i < lednum; i++)
  {
    for(byte j=0; j<3; j++)
    {
      set[j] = leds[i][j]*leds[i][3];   //led = (brightness*colour) = (lum*col)/255
      set[j] = set[j] << 8;             // div by 255 shortcut div by 256 via bit shift
    }
    cases.setPixelColor(i, (byte)set[1], (byte)set[2], (byte)set[3]);
  }
}


void detect()	// polls each external sensor and updates vars
{				// reset button managed by setleds()
	caseopen = digitalRead(sidepanel);
	if(digitalRead(powerbutton))
	{
		mode = mode_power_on;
		digitalWrite(powerpin, HIGH);
		delay(2);
		digitalWrite(powerpin,LOW);
	}

	if (pressresolve) buttonprs();

	if(Serial.available())
	{
		if(!conv()) Serial.println("Configuration failed!");
	}

}


void loop()
{
	detect();
	setleds();
	cases.show();

}
