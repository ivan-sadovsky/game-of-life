import arduino_random_emulator

if __name__ == '__main__':
    arduino_random_emulator.randomSeed(538563797)
    for i in range(3):
        print(arduino_random_emulator.random(100000))
    arduino_random_emulator.randomSeed(123582058)
    for i in range(3):
        print(arduino_random_emulator.random(10000, 20000))
