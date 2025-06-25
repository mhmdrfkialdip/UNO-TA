// ESP2: RX dari ESP1 TX (RX=16), TX ke ESP1 RX (TX=17)

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("ESP2 Siap menerima...");
}

void loop() {
  if (Serial2.available()) {
    String received = Serial2.readStringUntil('\n');
    Serial.print("Data diterima: ");
    Serial.println(received);
  }
}
