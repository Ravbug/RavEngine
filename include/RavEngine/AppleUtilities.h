#pragma once

/**
 Workaround for deadlock on metal. Manually creates the metal layer.
 @param wnd the Cocoa Window pointer
 @return pointer to the created metal layer.
 */
void *cbSetupMetalLayer(void *wnd);

/**
 Resize a metal layer manually. Required on iOS
 @param ptr the pointer to the CAMetalLayer
 @param width the new width, in points
 @param height the new height, in points
 */
void resizeMetalLayer(void* ptr, int width, int height);

/**
 Get the scale factor on macOS or iOS
 @param window the SDL window pointer
 @return the scale factor returned by the Apple API
 */
float GetWindowScaleFactor(void* window);

/**
 SDL opts-out of inertial scrolling on macOS. This function re-enables it.
 */
void enableSmoothScrolling();
