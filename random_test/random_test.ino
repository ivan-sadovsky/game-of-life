void setup() {
  Serial.begin(9600);

  randomSeed(538563797);
  for (byte i=0; i<3; i++) {
    Serial.print(random(100000)); Serial.print("\n");
  }
  randomSeed(123582058);
  for (byte i=0; i<3; i++) {
    Serial.print(random(10000, 20000)); Serial.print("\n");
  }
}

void loop() {
}
