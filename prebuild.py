Import("env")

# Ensure C++17 is enabled
env.Append(CXXFLAGS=["-std=gnu++17"])

# Add platform-specific flags for ESP32
if env.get("PIOPLATFORM", "") == "espressif32":
    env.Append(CPPDEFINES=[
        "CONFIG_ARDUHAL_LOG_COLORS",
        "ESP32"
    ])
    
    # Enable FreeRTOS threading features
    env.Append(CPPDEFINES=[
        "CONFIG_FREERTOS_ENABLE_BACKWARD_COMPATIBILITY",
        "CONFIG_FREERTOS_HZ=1000"
    ])

    # Optimize for performance
    env.Append(CCFLAGS=["-O2"])
    
    # Enable warnings
    env.Append(CCFLAGS=[
        "-Wall",
        "-Wextra",
        "-Werror=return-type"
    ])

# Debug build settings
if env.GetBuildType() == "debug":
    env.Append(CPPDEFINES=[
        "ENABLE_EVENT_DEBUG_LOGS=1",
        "CORE_DEBUG_LEVEL=5"
    ])
    env.Append(CCFLAGS=[
        "-g3",
        "-ggdb",
        "-fno-eliminate-unused-debug-types"
    ])

print("Build environment configured:")
print(f"Platform: {env.get('PIOPLATFORM', 'unknown')}")
print(f"Build type: {env.GetBuildType()}")
print(f"C++ flags: {env.get('CXXFLAGS', [])}")
print(f"Defines: {env.get('CPPDEFINES', [])}")
