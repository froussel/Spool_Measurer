/* Beta test code pour projet PIC IMT11 SpoolMeasurer */
/* Programme de test du materiel*/
/*
Liste des Bug et truc a mettre en place :
- Sur l'ecran Manual Run, si on met la vitesse à 0 on ne peut plus la remonté directement sans passer par un autre menu
- Sur Auto_Init la modification de la valeure cible ne fonctionne pas bien (a cause des variables commande_push et commande_waiting)
- Sur Autu_run le joystick gauche vas sur Finished et non sur pause idem pour le droit.
- Une fois la target atteint le programme continue (sur la fenetre Auto_Run)

A faire :
- commencer a codes les execptions et les mettres dans le ruuning_program ou loop
£
*/

#include <TimerOne.h>
#include <LiquidCrystal_I2C.h>

//******* Déclaration des Pin de l'arduino **********

	// PIN pour le Driver/Moteur A4988
	#define PIN_STEP  9
	#define PIN_DIR   8
	#define PIN_ENABLE_DRIVER 10

	LiquidCrystal_I2C lcd(0x27, 16, 2);// définit le type d'écran lcd avec connexion I2C sur les PIN SLA SCL

	//Pin du Joystick
	const int PIN_X 		= A0; // analog pin connected to X output
	const int PIN_Y 		= A1; // analog pin connected to Y output
	const int PIN_CLICK 	= 3;// Digital pin connected to Y output
	//Pin Fourche Optique
	const byte PIN_FOURCHE 	= 2; //Digital Pin connected to optical switch

//******* Déclaration des Variables de statu *************
	//Variable des bouton joystick
	bool X_PLUS 			= 0;
	bool X_MOIN 			= 0;
	bool Y_PLUS 			= 0;
	bool Y_MOIN 			= 0;
	bool CLICK 				= 0;
	bool commande_waiting	= 1;// Variable pour attendre que le joystick se remete en 0
	bool commande_push		= 0;// Variable pour push une commande joystick

	//****** Variable de statu ******

	// Démare/arrete le moteur a une vitesse donnée
	bool MOTOR_RUN 			= 0;
	// Active/Désactive le moteur et son couple
	bool MOTOR_ENABLE 		= 0;
	

	//****** Variable de de fenetre IHM******

	//********| Spool Measurer |*********//
	//********|<-Manual  Auto->|*********//
	bool window_Menu 		= 1;
	//********|M.   Lgt:  1.93m|*********//
	//********|  Speed:0   rpm |*********//
	bool window_Manual_run 	= 0;
	//********|M.   Lgt:  1.93m|*********//
	//********|<-Menu   Start->|*********//
	bool window_Manual_init	= 0;
	//********|     Paused     |*********//
	//********|     23.01 m    |*********//
	bool window_Manu_paused	= 0;
	//********| Choose length! |*********//
	//********| Lgt:  23.01 m  |*********//
	bool window_Auto_init 	= 0;
	//********| 002.22m/230.00m|*********//
	//********|  Speed:0   rpm |*********//
	bool window_Auto_run 	= 0;
	//********|     Paused     |*********//
	//********|     23.01 m    |*********//
	bool window_Auto_paused	= 0;
	//********|   Finished !   |*********//
	//********|     23.01 m    |*********//
	bool window_Finish 		= 0;
	//********| System Failure |*********//
	//********|<-Abort  Retry->|*********//
	bool window_Fail 		= 0;


//******* Variable pour le fonctinnement moteur **********
	int actual_speed;
	int actual_direction;
	int target_speed;
	int ticks;
	int tick_count;
	// Constante de direction du moteur
	#define FORWARD   HIGH
	#define BACKWARD  LOW

//*********** Parametre de vitesse du moteur ***************

	const int speed_ticks[] = {-1, 400,375,350,325,300,275,250,225,200,175,150,125,110,100,90,85,80,75,70,65,60,55,50};
	size_t max_speed = (sizeof(speed_ticks) / sizeof(speed_ticks[0])) -1 ; // nombre de variable dans le tableau



//******* Variable pour la fonction de mesure **********

	//Compteur du nombre de steps de la roue codeuse
	unsigned int  counter_steps=0;
	//Mesure de la longueure filament à l'instant T (afficher sur l'IHM)
	float measurement = 0; 
	//La mesure ciblé de longueure de filament
	float measurement_target = 1; 
	
	
	//                           IIIII
	//                           IIIII
	//     Variable pour         IIIII
	//      la calibration     IIIIIIIII
	//                          IIIIIII
	//                           IIII
	//                            II

	//périmetre de la roue denté de mesure en mm
	const float perimeter_gear = 0.0339;
	//nombre de fenetre sur la roue codeuse
	const int encoder_hole = 2;
	//Course de la bobine lors de la décélération de 75 RPM à 0
	const float distance_deceleration_high = 40.0;
	//Course de la bobine lors de la décélération de 35 RPM à 0
	const float distance_deceleration_low = 20.0;

//******* Variable pour la fonction de TimeOut **********

	//Variable du dernier temps de mesure via millis()
	unsigned long previous_time_measurement = 0;
	//Temps en millisecond avant timeout (2s)
	const unsigned long timeout = 2000;

/*
 * =============================================================================================
 * ========================================= Setup =============================================
 * =============================================================================================
*/
void setup() {

	digitalWrite(PIN_ENABLE_DRIVER, HIGH);// Désactivation du moteur avant le setup

	// initial values
  	actual_speed = 0;
  	target_speed = max_speed/2;
  	actual_direction = BACKWARD;
  	tick_count = 0;
  	ticks = -1;
	
	// Init du Timer1, interrupt toute les 0.1ms pour lancer un step moteur
	Timer1.initialize(20);
	Timer1.attachInterrupt(timerMotor);
	
	// Init de l'interrupt de compteur de la fourche optique en fonction du soulevement du signal
	attachInterrupt(digitalPinToInterrupt(PIN_FOURCHE), measure_filament, RISING);

	//Initialisation du LCD I2C
	lcd.init();
	lcd.backlight();

	//Initialisation des PIN
  	
	// pins DRIVER
  	pinMode(PIN_ENABLE_DRIVER, OUTPUT);
	digitalWrite(PIN_ENABLE_DRIVER, HIGH);// Désactivation du moteur
  	pinMode(PIN_STEP, OUTPUT);
  	pinMode(PIN_DIR, OUTPUT);
	digitalWrite(PIN_DIR, actual_direction);

  	// Pin Fourche ??????????????????????????????????????????????????????
  	pinMode(PIN_FOURCHE, INPUT_PULLUP);

}
/*
 * =============================================================================================
 * ======================================== Main Loop ==========================================
 * =============================================================================================
*/
void loop() {

	measurement =	counter_steps * perimeter_gear/encoder_hole;

	read_joystick();	// Lectue Joystick
	ruuning_program();	// Boucle programme principal
	updateLCD();		// Mise a jours du LCD

	if (MOTOR_ENABLE)//Activation du moteur
	{
		digitalWrite(PIN_ENABLE_DRIVER, LOW);
	}
	else//Désactivation du moteur
	{
		digitalWrite(PIN_ENABLE_DRIVER, HIGH);
	}

	if (MOTOR_RUN)//Augmentation de la vitesse à la target
	{
		increase_speed();
	}
	else//Diminution de la vitesse du moteur jusqu'à l'arret
	{
		decrease_speed();
	}

	//Vérification que nous ne somme pas en TimeOut (plus de mesure depuis X secondes)
	if(previous_time_measurement + timeout > millis())
	{
		if (window_Auto_run || window_Manual_run)
		{
			window_Fail = 1;
		}
	}

}

/********************************* Programme principal ****************************************
 * 
 *  - fonctionement du programme suivant la séquence IHM (voir fichier)
 * 
************************************************************************************************/
void ruuning_program() {
	if (!commande_push || !commande_waiting ){// exclusion de windows fail ?
		return; //<- A tester VS le fonctionnement du programme en auto
	}
	
	
	if(window_Fail){
		if(Y_MOIN){
			resetIHM();
			window_Finish = 1;
		}
		if(Y_PLUS){
			if(window_Auto_run || window_Auto_paused || window_Auto_init)
			{
				MOTOR_RUN = 1 ;
				window_Fail = 0;
			}
			else if (window_Manu_paused || window_Manual_init || window_Manual_run)
			{
				MOTOR_RUN = 1 ;
				window_Fail = 0;
			}
		}
	}
	else if (window_Menu){
		counter_steps = 0;
		MOTOR_RUN = 0;
		if(Y_MOIN){
			resetIHM();
			window_Manual_init = 1;
		}
		else if(Y_PLUS){
			resetIHM();
			window_Auto_init = 1;
		}
	}
	else if(window_Manual_init)
	{
		if(Y_MOIN){
			resetIHM();
			window_Menu = 1;
		}
		else if(Y_PLUS){
			resetIHM();
			window_Manual_run = 1;
			MOTOR_RUN = 1;
			
		}
	}
	else if(window_Manual_run)
	{
		if(Y_MOIN){
			MOTOR_RUN = 0;
			resetIHM();
			window_Manu_paused = 1;
		}
	}
	else if(window_Manu_paused)
	{
		if(Y_MOIN){
			resetIHM();
			window_Finish = 1;
		}
		if(Y_PLUS){
			resetIHM();
			MOTOR_RUN = 1 ;
			window_Manual_run = 1;
		}
	}
	else if(window_Finish)
	{
		if(Y_MOIN || Y_PLUS){
			resetIHM();
			window_Menu = 1;
		}
	}
	else if(window_Auto_init){
		if(Y_MOIN){
			resetIHM();
			window_Menu = 1;
		}
		else if(Y_PLUS && measurement_target>0){
			resetIHM();
			window_Auto_run = 1;
			MOTOR_RUN = 1;
		}
	}
  	else if(window_Auto_run){
		if(Y_MOIN){
			MOTOR_RUN = 0;
			resetIHM();
			window_Auto_paused = 1;
		}
		
		/* Code pour le fonctionnement du moteur en fonction de la cible a mesurer*/
		if(measurement_target<measurement-distance_deceleration_high)
		{

		}
		else if(measurement_target<measurement-distance_deceleration_low)
		{

		}
		else if(measurement_target >= measurement)
		{
			MOTOR_RUN = 0;
			resetIHM();
			window_Finish = 1;
		}
	}
	else if(window_Auto_paused){
		if(Y_MOIN){
			resetIHM();
			window_Finish = 1;
		}
		if(Y_PLUS){
			resetIHM();
			MOTOR_RUN = 1 ;
			window_Auto_run = 1;
		}
	}
	commande_waiting = 0;
 
}
/********************************* Fonction Diminution de la vitesse Moteur *********************
 * 
 * - Augmentation de la vitesse moteur en fonction des valeurs du tableau speed_ticks
 * 
************************************************************************************************/
void increase_speed() {
  
  if(actual_speed < target_speed) {
    actual_speed += 1;
    tick_count = 0;
    ticks = speed_ticks[actual_speed];
  }
  else if (actual_speed > target_speed){
	actual_speed -= 1;
	tick_count = 0;
	ticks = speed_ticks[actual_speed];
  }
}

/********************************* Fonction Diminution de la vitesse Moteur *********************
 * 
 * - Diminution de la vitesse moteur en fonction des valeurs du tableau speed_ticks
 * 
************************************************************************************************/
void decrease_speed() {
  
  if(actual_speed > 0) {
    actual_speed -= 1;
    tick_count = 0;
    ticks = speed_ticks[actual_speed];
  } 
}

/********************************* Fonction arret d'urgence Moteur ******************************
 * 
 * - Non utilisé
 * 
************************************************************************************************/
void emergency_stop() {
  actual_speed = 0;
  tick_count = 0;
  ticks = speed_ticks[actual_speed / 5];
}

/********************************* Fonction TimerMotor *****************************************
 *
 * - Fonction lancé toute les 20ms avec au Timer1
 * - Fait tourner le moteur un tour en fonction de la vitesse
 * 
************************************************************************************************/
void timerMotor() {
	if(actual_speed == 0) return;

	digitalWrite(PIN_ENABLE_DRIVER, LOW);// Activation du moteur
	MOTOR_ENABLE = 1;//Flag motor en marche

	tick_count++;

	if(tick_count == ticks) {  
		// 1 step du moteur
		digitalWrite(PIN_STEP, HIGH);
		digitalWrite(PIN_STEP, LOW);

		tick_count = 0;
	}
}



/********************************* Fonction de compteur de tour par interupt *******************
 *
 *  - Ajoute 1 à la variable compteur_hole
 * - la roue codeuse comporte 2 Hole --> + 2 au compteur_hole = 1 tours
 * 
************************************************************************************************/
void measure_filament() {
	if (digitalRead(PIN_FOURCHE)){
		counter_steps += 1;
		previous_time_measurement = millis();
	}
}


/********************************* Fonction de Lecture du Joystick *****************************
 *
 *  - Lit le joystick en X et Y sur les pin Analog
 * - Mets à jours l'IHM en fonction des fenetre dans lequel ce trouve l'utilisateur
 * - Mets à jours les Flags Y_PLUS et Y_MOINS
 * 
************************************************************************************************/

void read_joystick() {

	int XValue = analogRead(PIN_X);     // Read the analog value from The X-axis from the joystick
	int YValue = analogRead(PIN_Y);     // Read the analog value from The Y-axis from the joystick

	// ************************* Analyse de l'axe Y *********************************

  	if (!Y_PLUS && !Y_MOIN){
		if (YValue < 10){ // joystick Y - -> 
			Y_MOIN = 1;
			Y_PLUS = 0;
		}
		else if (YValue > 800 ){ // joystick Y +  -> 
			Y_MOIN = 0;
			Y_PLUS = 1;
		}
	}
	else if (YValue < 800 && YValue > 50){        // Y en home position      
		Y_MOIN = 0;
		Y_PLUS = 0;
	} 

	// ************************* Analyse de l'axe X *********************************

    if (XValue < 400 && XValue > 10){ // joystick X - légée -> reduce motor speed ou longueur
		if (!window_Auto_init)
		{	

			if (MOTOR_RUN && target_speed > 5 ){
				target_speed = target_speed - 1;
			}
		}

    }
    else if (XValue < 10){ // joystick X - -> Stop motor ou choisir la longueur
        if (window_Auto_init)
		{	
			if(measurement_target >0)
			measurement_target = target_speed - 1;
			commande_waiting = 1;
			commande_push = 0;
			return;
		}
		else
		{	
			if (MOTOR_RUN && target_speed > 5 ){
				target_speed = target_speed - 1;
			}
			else if (MOTOR_RUN && target_speed <= 5 ){
				MOTOR_RUN = 0;
			}
			else if ( actual_speed <= 0){
				MOTOR_ENABLE = 0;
			}
		}
    }
   	else if (XValue > 800){ // joystick X + -> Start motor or Speed OR longueure
      	if (window_Auto_init)
		{	
			if(measurement_target >0)
			measurement_target = target_speed + 1;
			return;
		}
		else
		{
			if (!MOTOR_RUN){
				target_speed = max_speed/2;
				MOTOR_RUN = 1;
			}
			if (MOTOR_RUN){
				if (target_speed < max_speed){
					target_speed += 1;
				}
			}
		}
    }

	// ** Analyse des Commandes **
	// Vérifier q'une commande est activé et mettre le programme en attente de 
	// la prochaine commande.
	if (commande_waiting)
	{
		if (Y_PLUS || Y_MOIN)
		{
			commande_push = 1;
		}
	}
	else if(!commande_waiting)
	{
		if (!Y_PLUS && !Y_MOIN)
		{
			commande_waiting = 1;
			commande_push = 0;
		}
	}

	
}

/********************************* Update LCD **************************************************
 *
 * - Fonction qui met a jours l'IHM en fonction de la fenetre dans lequels on est
 * - Nom des fenetres :
 * 		- window_Menu
 * 		- window_Manual_run
 * 		- window_Auto_init
 * 		- window_Auto_run
 * 		- window_Paused
 * 		- window_Finish
 * 		- window_Fail
 *  
 * 
************************************************************************************************/
void updateLCD() {
	lcd.setCursor(0,0);
	lcd.print(commande_push);
	lcd.print(commande_waiting);

	if(window_Fail)
	{
		//********| System Failure |*********//
		//********|<-Abort  Retry->|*********//
		lcd.setCursor(0, 0);
		lcd.print(" System Failure ");
		lcd.setCursor(0, 1);
		lcd.print("<-Abort  Retry->");
	}
	else if (window_Menu)
	{
		//********| Spool Measurer |*********//
		//********|<-Manual  Auto->|*********//
		lcd.setCursor(0, 0);
		lcd.print(" Spool Measurer ");
		lcd.setCursor(0, 1);
		lcd.print("<-Manual  Auto->");
	}
	else if (window_Manual_run)
	{
		//********|M.   Lgt:  1.93m|*********//
		//********|  Speed:0   rpm |*********//
		lcd.setCursor(0, 0);
		lcd.print("M.   Lgt:");
		lcd.setCursor(9, 0);
		lcd.print("      ");
		lcd.setCursor(9, 0);
		lcd.print(measurement);
		lcd.setCursor(15, 0);
		lcd.print("m");
		lcd.setCursor(0, 1);
		lcd.print("  Speed:");
		lcd.setCursor(9, 1);
		lcd.print("    ");
		lcd.setCursor(9, 1);
		lcd.print(actual_speed);
		lcd.setCursor(12, 1);
		lcd.print("rpm ");
	}
	else if (window_Manual_init)
	{
		//********|M.   Lgt:  1.93m|*********//
		//********|<-Menu   Start->|*********//
		lcd.setCursor(0, 0);
		lcd.print("M.   Lgt:");
		lcd.setCursor(9, 0);
		lcd.print("      ");
		lcd.setCursor(9, 0);
		lcd.print(measurement);
		lcd.setCursor(15, 0);
		lcd.print("m");
		lcd.setCursor(0, 1);
		lcd.print("<-Menu   Start->");
		
	}
	else if (window_Auto_init)
	{
		//********| Choose length! |*********//
		//********| Lgt:  23.01 m  |*********//
		lcd.setCursor(0, 0);
		lcd.print(" Choose length! ");
		lcd.setCursor(0, 1);
		lcd.print(" Lgt: ");
		lcd.setCursor(6, 1);
		lcd.print("      ");
		lcd.setCursor(6, 1);
		lcd.print(measurement_target);
		lcd.setCursor(12, 1);
		lcd.print(" m  ");
	}
	else if (window_Auto_run)
	{
		//********| 002.22m/230.00m|*********//
		//********|  Speed:0   rpm |*********//
		lcd.setCursor(0, 0);
		lcd.print(" ");
		lcd.setCursor(1, 0);
		lcd.print("      ");
		lcd.setCursor(1, 0);
		lcd.print(measurement);
		lcd.setCursor(7, 0);
		lcd.print("m/");
		lcd.setCursor(9, 0);
		lcd.print("      ");
		lcd.setCursor(9, 0);
		lcd.print(measurement_target);
		lcd.setCursor(15, 0);
		lcd.print("m");

		lcd.setCursor(0, 1);
		lcd.print("  Speed:");
		lcd.setCursor(9, 1);
		lcd.print("    ");
		lcd.setCursor(9, 1);
		lcd.print(actual_speed);
		lcd.setCursor(12, 1);
		lcd.print("rpm ");
	}
	else if (window_Auto_paused || window_Manu_paused)
	{
		//********|     Paused     |*********//
		//********|     23.01 m    |*********//
		lcd.setCursor(0, 0);
		lcd.print("     PAUSED     ");
		lcd.setCursor(0, 1);
		lcd.print("    ");
		lcd.setCursor(4, 1);
		lcd.print("      ");
		lcd.setCursor(4, 1);
		lcd.print(measurement);
		lcd.setCursor(9, 1);
		lcd.print(" m    ");
	}
	else if (window_Finish)
	{
		//********|   Finished !   |*********//
		//********|     23.01 m    |*********//
		lcd.setCursor(0, 0);
		lcd.print("   Finished !   ");
		lcd.setCursor(0, 1);
		lcd.print("    ");
		lcd.setCursor(4, 1);
		lcd.print("      ");
		lcd.setCursor(4, 1);
		lcd.print(measurement);
		lcd.setCursor(9, 1);
		lcd.print(" m    ");
	}
	else
	{
		//********| System Failure |*********//
		//********|<-Abort  Retry->|*********//
		lcd.setCursor(0, 0);
		lcd.print(" System Failure ");
		lcd.setCursor(0, 1);
		lcd.print("<-Abort  Retry->");
	}

	/*lcd.setCursor(0,0);
	lcd.print("Speed: ");
	lcd.print(actual_speed);
	lcd.print("RPM ");

	lcd.setCursor(0,1);
	lcd.print(counter_steps);
	lcd.print("   ");*/
	
} 

/********************************* Reset ecran IHM **************************************************
 *
 * - Fonction remet toute les variables d'écrans LCD à 0
*
************************************************************************************************/
void resetIHM() {
	window_Menu 		= 0;
	window_Manual_run 	= 0;
	window_Manual_init	= 0;
	window_Manu_paused	= 0;
	window_Auto_init 	= 0;
	window_Auto_run 	= 0;
	window_Auto_paused	= 0;
	window_Finish 		= 0;
	window_Fail 		= 0;
}