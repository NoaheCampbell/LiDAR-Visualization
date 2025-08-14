#ifndef BUTTON_UTILS_H
#define BUTTON_UTILS_H

#include <cstdint>
#include <string>
#include <vector>
#include "Constants.h"

/**
 * Utility functions for manipulating the 4-button system
 * Each rover has 4 buttons represented as bit flags in a uint8_t
 */
class ButtonUtils {
public:
    /**
     * Check if a specific button is pressed in the button state
     * @param buttonState the button state byte
     * @param buttonNumber button number (1-4)
     * @return true if the button is pressed
     */
    static bool isButtonPressed(uint8_t buttonState, int buttonNumber);
    
    /**
     * Set a specific button state
     * @param buttonState current button state byte
     * @param buttonNumber button number (1-4)
     * @param pressed whether the button should be pressed
     * @return new button state
     */
    static uint8_t setButtonState(uint8_t buttonState, int buttonNumber, bool pressed);
    
    /**
     * Toggle a specific button state
     * @param buttonState current button state byte
     * @param buttonNumber button number (1-4)
     * @return new button state with the button toggled
     */
    static uint8_t toggleButton(uint8_t buttonState, int buttonNumber);
    
    /**
     * Get the bit mask for a specific button number
     * @param buttonNumber button number (1-4)
     * @return bit mask for the button, or 0 if invalid
     */
    static uint8_t getButtonMask(int buttonNumber);
    
    /**
     * Count how many buttons are currently pressed
     * @param buttonState button state byte
     * @return number of pressed buttons (0-4)
     */
    static int countPressedButtons(uint8_t buttonState);
    
    /**
     * Get a list of all pressed button numbers
     * @param buttonState button state byte
     * @return vector of pressed button numbers (1-4)
     */
    static std::vector<int> getPressedButtons(uint8_t buttonState);
    
    /**
     * Clear all button states (set all to unpressed)
     * @return button state with all buttons unpressed (0x00)
     */
    static uint8_t clearAllButtons();
    
    /**
     * Set all button states (set all to pressed)
     * @return button state with all buttons pressed (0x0F)
     */
    static uint8_t setAllButtons();
    
    /**
     * Check if any buttons are pressed
     * @param buttonState button state byte
     * @return true if at least one button is pressed
     */
    static bool anyButtonPressed(uint8_t buttonState);
    
    /**
     * Check if all buttons are pressed
     * @param buttonState button state byte
     * @return true if all buttons are pressed
     */
    static bool allButtonsPressed(uint8_t buttonState);
    
    /**
     * Convert button state to human-readable string
     * @param buttonState button state byte
     * @return string representation (e.g., "1,3" for buttons 1 and 3 pressed)
     */
    static std::string buttonStateToString(uint8_t buttonState);
    
    /**
     * Parse button state from string
     * @param stateString string representation of button state (e.g., "1,3")
     * @return button state byte, or 0 if parsing fails
     */
    static uint8_t stringToButtonState(const std::string& stateString);
    
    /**
     * Validate that a button number is in the valid range
     * @param buttonNumber button number to validate
     * @return true if button number is valid (1-4)
     */
    static bool isValidButtonNumber(int buttonNumber);
    
    /**
     * Validate that a button state is within valid range
     * @param buttonState button state byte to validate
     * @return true if button state is valid (0x00-0x0F)
     */
    static bool isValidButtonState(uint8_t buttonState);
    
    /**
     * Get button state differences between two states
     * @param oldState previous button state
     * @param newState current button state
     * @return pair of (pressed_buttons, released_buttons) as bit masks
     */
    static std::pair<uint8_t, uint8_t> getButtonChanges(uint8_t oldState, uint8_t newState);
    
    /**
     * Create a button command mask from individual button states
     * @param button1 button 1 state
     * @param button2 button 2 state
     * @param button3 button 3 state
     * @param button4 button 4 state
     * @return combined button state byte
     */
    static uint8_t createButtonMask(bool button1, bool button2, bool button3, bool button4);

private:
    // Internal helper to get mask for button number (0-based internal)
    static uint8_t getButtonMaskInternal(int buttonIndex);
};

#endif // BUTTON_UTILS_H