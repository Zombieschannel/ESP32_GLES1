
idf_component_register(SRCS "GLES.cpp" "EGL.cpp" "Context.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES arduino TFT_eSPI)
