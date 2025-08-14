#include "ButtonUtils.h"
#include <sstream>
#include <algorithm>

bool ButtonUtils::isButtonPressed(uint8_t buttonState, int buttonNumber) {
    if (!isValidButtonNumber(buttonNumber)) {
        return false;
    }
    
    uint8_t mask = getButtonMask(buttonNumber);
    return (buttonState & mask) != 0;
}

uint8_t ButtonUtils::setButtonState(uint8_t buttonState, int buttonNumber, bool pressed) {
    if (!isValidButtonNumber(buttonNumber)) {
        return buttonState;
    }
    
    uint8_t mask = getButtonMask(buttonNumber);
    
    if (pressed) {
        return buttonState | mask;  // Set the bit
    } else {
        return buttonState & ~mask; // Clear the bit
    }
}

uint8_t ButtonUtils::toggleButton(uint8_t buttonState, int buttonNumber) {
    if (!isValidButtonNumber(buttonNumber)) {
        return buttonState;
    }
    
    uint8_t mask = getButtonMask(buttonNumber);
    return buttonState ^ mask;  // XOR to toggle the bit
}

uint8_t ButtonUtils::getButtonMask(int buttonNumber) {
    switch (buttonNumber) {
        case 1: return RoverConfig::BUTTON_1_MASK;
        case 2: return RoverConfig::BUTTON_2_MASK;
        case 3: return RoverConfig::BUTTON_3_MASK;
        case 4: return RoverConfig::BUTTON_4_MASK;
        default: return 0;
    }
}

int ButtonUtils::countPressedButtons(uint8_t buttonState) {
    int count = 0;
    uint8_t validState = buttonState & RoverConfig::ALL_BUTTONS_MASK;
    
    // Count set bits using Brian Kernighan's algorithm
    while (validState) {
        count++;
        validState &= (validState - 1);  // Clear the lowest set bit
    }
    
    return count;
}

std::vector<int> ButtonUtils::getPressedButtons(uint8_t buttonState) {
    std::vector<int> pressedButtons;
    
    for (int i = 1; i <= RoverConfig::BUTTON_COUNT; ++i) {
        if (isButtonPressed(buttonState, i)) {
            pressedButtons.push_back(i);
        }
    }
    
    return pressedButtons;
}

uint8_t ButtonUtils::clearAllButtons() {
    return 0x00;
}

uint8_t ButtonUtils::setAllButtons() {
    return RoverConfig::ALL_BUTTONS_MASK;
}

bool ButtonUtils::anyButtonPressed(uint8_t buttonState) {
    return (buttonState & RoverConfig::ALL_BUTTONS_MASK) != 0;
}

bool ButtonUtils::allButtonsPressed(uint8_t buttonState) {
    return (buttonState & RoverConfig::ALL_BUTTONS_MASK) == RoverConfig::ALL_BUTTONS_MASK;
}

std::string ButtonUtils::buttonStateToString(uint8_t buttonState) {
    std::vector<int> pressed = getPressedButtons(buttonState);
    
    if (pressed.empty()) {
        return "none";
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < pressed.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << pressed[i];
    }
    
    return oss.str();
}

uint8_t ButtonUtils::stringToButtonState(const std::string& stateString) {
    if (stateString.empty() || stateString == "none") {
        return 0x00;
    }
    
    uint8_t result = 0x00;
    std::istringstream iss(stateString);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        // Remove whitespace
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
        
        try {
            int buttonNumber = std::stoi(token);
            if (isValidButtonNumber(buttonNumber)) {
                result = setButtonState(result, buttonNumber, true);
            }
        } catch (const std::exception&) {
            // Ignore invalid tokens
        }
    }
    
    return result;
}

bool ButtonUtils::isValidButtonNumber(int buttonNumber) {
    return buttonNumber >= 1 && buttonNumber <= RoverConfig::BUTTON_COUNT;
}

bool ButtonUtils::isValidButtonState(uint8_t buttonState) {
    return (buttonState & ~RoverConfig::ALL_BUTTONS_MASK) == 0;
}

std::pair<uint8_t, uint8_t> ButtonUtils::getButtonChanges(uint8_t oldState, uint8_t newState) {
    uint8_t validOld = oldState & RoverConfig::ALL_BUTTONS_MASK;
    uint8_t validNew = newState & RoverConfig::ALL_BUTTONS_MASK;
    
    uint8_t pressed = validNew & ~validOld;   // Buttons that became pressed
    uint8_t released = validOld & ~validNew;  // Buttons that became released
    
    return std::make_pair(pressed, released);
}

uint8_t ButtonUtils::createButtonMask(bool button1, bool button2, bool button3, bool button4) {
    uint8_t result = 0x00;
    
    if (button1) result |= RoverConfig::BUTTON_1_MASK;
    if (button2) result |= RoverConfig::BUTTON_2_MASK;
    if (button3) result |= RoverConfig::BUTTON_3_MASK;
    if (button4) result |= RoverConfig::BUTTON_4_MASK;
    
    return result;
}

uint8_t ButtonUtils::getButtonMaskInternal(int buttonIndex) {
    // Convert 0-based index to 1-based button number and get mask
    return getButtonMask(buttonIndex + 1);
}