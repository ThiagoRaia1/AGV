  #include <Arduino.h>

  // =====================================================
  // CONFIGURAÇÃO DOS SENSORES
  // =====================================================

  #define IR_SENSOR_RIGHT 33
  #define IR_SENSOR_LEFT  32

  // Velocidade dos motores (0 a 255)
  #define MOTOR_SPEED 160

  // =====================================================
  // LEDs DE STATUS
  // =====================================================

  const int parado = 2;
  const int virando = 4;
  const int reto = 13;

  // =====================================================
  // MOTOR DIREITO - L298N
  // =====================================================

  int enableRightMotor = 27;
  int rightMotorPin1 = 25;
  int rightMotorPin2 = 26;

  // =====================================================
  // MOTOR ESQUERDO - L298N
  // =====================================================

  int enableLeftMotor = 14;
  int leftMotorPin1 = 18;
  int leftMotorPin2 = 19;

  // =====================================================
  // CONFIGURAÇÃO INICIAL
  // =====================================================

  void setup()
  {
    Serial.begin(115200);

    // Configuração dos pinos do motor direito
    pinMode(enableRightMotor, OUTPUT);
    pinMode(rightMotorPin1, OUTPUT);
    pinMode(rightMotorPin2, OUTPUT);

    // Configuração dos pinos do motor esquerdo
    pinMode(enableLeftMotor, OUTPUT);
    pinMode(leftMotorPin1, OUTPUT);
    pinMode(leftMotorPin2, OUTPUT);

    // Configuração dos sensores
    pinMode(IR_SENSOR_RIGHT, INPUT);
    pinMode(IR_SENSOR_LEFT, INPUT);

    // Configuração dos LEDs
    pinMode(parado, OUTPUT);
    pinMode(virando, OUTPUT);
    pinMode(reto, OUTPUT);

    // Configuração PWM do ESP32
    analogWriteResolution(enableRightMotor, 8);
    analogWriteResolution(enableLeftMotor, 8);

    analogWriteFrequency(enableRightMotor, 5000);
    analogWriteFrequency(enableLeftMotor, 5000);

    // Inicia com os motores parados
    rotateMotor(0, 0);

    Serial.println("================================");
    Serial.println("SEGUIDOR DE LINHA ESP32");
    Serial.println("Sistema iniciado!");
    Serial.println("================================");
  }

  // =====================================================
  // LOOP PRINCIPAL
  // =====================================================

  void loop()
  {
    // Leitura dos sensores
    int rightIRSensorValue = digitalRead(IR_SENSOR_RIGHT);
    int leftIRSensorValue = digitalRead(IR_SENSOR_LEFT);

    // Exibe os valores no monitor serial
    Serial.print("Sensor Direito: ");
    Serial.print(rightIRSensorValue);

    Serial.print(" | Sensor Esquerdo: ");
    Serial.println(leftIRSensorValue);

    // =================================================
    // NENHUM SENSOR DETECTA PRETO
    // =================================================

    if (rightIRSensorValue == LOW &&
        leftIRSensorValue == LOW)
    {
      rotateMotor(MOTOR_SPEED, MOTOR_SPEED);

      digitalWrite(parado, LOW);
      digitalWrite(virando, LOW);
      digitalWrite(reto, HIGH);
    }

    // =================================================
    // SENSOR DIREITO DETECTA PRETO
    // =================================================

    else if (rightIRSensorValue == HIGH &&
            leftIRSensorValue == LOW)
    {
      rotateMotor(MOTOR_SPEED, -MOTOR_SPEED);

      digitalWrite(parado, LOW);
      digitalWrite(virando, HIGH);
      digitalWrite(reto, LOW);
    }

    // =================================================
    // SENSOR ESQUERDO DETECTA PRETO
    // =================================================

    else if (rightIRSensorValue == LOW &&
            leftIRSensorValue == HIGH)
    {
      rotateMotor(-MOTOR_SPEED, MOTOR_SPEED);

      digitalWrite(parado, LOW);
      digitalWrite(virando, HIGH);
      digitalWrite(reto, LOW);
    }

    // =================================================
    // AMBOS OS SENSORES DETECTAM PRETO
    // =================================================

    else
    {
      rotateMotor(0, 0);
      delay(2000);
      
      rotateMotor(MOTOR_SPEED, MOTOR_SPEED);


      loop();

      digitalWrite(parado, HIGH);
      digitalWrite(virando, LOW);
      digitalWrite(reto, LOW);
    }

    delay(10);
  }

  // =====================================================
  // FUNÇÃO DE CONTROLE DOS MOTORES
  // =====================================================

  void rotateMotor(int rightMotorSpeed, int leftMotorSpeed)
  {
    // =================================================
    // MOTOR DIREITO
    // =================================================

    if (rightMotorSpeed < 0)
    {
      digitalWrite(rightMotorPin1, LOW);
      digitalWrite(rightMotorPin2, HIGH);
    }
    else if (rightMotorSpeed > 0)
    {
      digitalWrite(rightMotorPin1, HIGH);
      digitalWrite(rightMotorPin2, LOW);
    }
    else
    {
      digitalWrite(rightMotorPin1, LOW);
      digitalWrite(rightMotorPin2, LOW);
    }

    // =================================================
    // MOTOR ESQUERDO
    // =================================================

    if (leftMotorSpeed < 0)
    {
      digitalWrite(leftMotorPin1, LOW);
      digitalWrite(leftMotorPin2, HIGH);
    }
    else if (leftMotorSpeed > 0)
    {
      digitalWrite(leftMotorPin1, HIGH);
      digitalWrite(leftMotorPin2, LOW);
    }
    else
    {
      digitalWrite(leftMotorPin1, LOW);
      digitalWrite(leftMotorPin2, LOW);
    }

    // =================================================
    // CONTROLE DE VELOCIDADE PWM
    // =================================================

    analogWrite(
      enableRightMotor,
      constrain(abs(rightMotorSpeed), 0, 255)
    );

    analogWrite(
      enableLeftMotor,
      constrain(abs(leftMotorSpeed), 0, 255)
    );
  }
