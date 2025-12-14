# knock-detector-ml
A door-knock detection device that uses an ML model to discern between door knocks and ambient noise.

<p align="center">
  <img src="https://github.com/user-attachments/assets/9db41e1b-908e-4af1-a69f-3e3c85a14461" width="400">
  <img src="https://github.com/user-attachments/assets/2d6b2827-6c7a-40c5-898d-61aa2a37c74b" width="400">
</p>

## Introduction
Many professors have students or other faculty who want to drop by their office to chat with them about important things like deadlines, class content, and follow ups to unread emails. However, many professors are often away from their offices, with no way of notifying the person where they are or when they will be back. The purpose of this project is to create a device that helps you detect a door knock (from a student or faculty) and discern that from ambient noise.

## Details
This project was built with both hardware and software components. The following hardware components were used:
- [Breadboard (2" x 3")](https://www.digikey.com/en/products/detail/dfrobot/FIT0096/7597069?gclsrc=aw.ds&gad_source=1&gad_campaignid=20232005509&gbraid=0AAAAADrbLli3Zre4ddlozLmGmpiwyJ41o&gclid=Cj0KCQiAuvTJBhCwARIsAL6DemgQf9n63cSaX4UHFFwtdKnLwFmu5tf8AWYSMNTi6E1qa-nBFHN2O5AaAvsxEALw_wcB)
- [IMU (MPU-6050)](https://www.amazon.com/Gy-521-MPU-6050-MPU6050-Sensors-Accelerometer/dp/B008BOPN40)
- [I2S Microphone](https://tinyurl.com/y43e6r94)
- [ESP32 Microcontroller](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-DEVKITC-32E/12091810?gclsrc=aw.ds&gad_source=1&gad_campaignid=20243136172&gbraid=0AAAAADrbLljTnHpUbhJn7eQMhVgfmvqop&gclid=Cj0KCQiAuvTJBhCwARIsAL6DemgF6fxXwYMWDpEv_bgImpCIXCN3ZPwfsMf7CKLE40MVjdao0SXQoEYaAsT7EALw_wcB)

The following software tools/frameworks were used:
- Arduino
- Edge Impulse
- CMake
- MinGW32
- Built with C++, C, Python

## Instructions for Wi-Fi setup
1. Under firmware/include, you should see a C header file called example_credentials.h. Replace the Wi-Fi SSID, password, and your IP Address with your own information (IP address should be the IPv4 address of your computer)
2. Rename example_credentials.h to credentials.h. The gitignore file should ignore the credentials.h file when pushing to your GitHub repo, so your information will not be leaked to the public.

## Results
I used Edge Impulse to create a ML library that I imported into this project. The model accuracy (as of 12/14/2025) is 97.4%; from field tests, the device detects knocks very well. Noisy inputs such as opening and closing the door, bumping into the door, etc. do not trigger the device; only true knocks do.

## Demo Video
Link: https://www.youtube.com/watch?v=u8AAG0BuPYU
