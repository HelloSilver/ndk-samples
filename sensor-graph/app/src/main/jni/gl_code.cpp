/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <string>

#include <android/asset_manager_jni.h>
#include <android/sensor.h>

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

const int LOOPER_ID_USER = 3;
const int SENSOR_HISTORY_LENGTH = 100;
const int SENSOR_REFRESH_RATE = 100;
const float SENSOR_FILTER_ALPHA = 0.1f;

class SensorGraphDemo {

    std::string vertexShaderSource;
    std::string fragmentShaderSource;
    ASensorManager *sensorManager;
    const ASensor *accelerometer;
    ASensorEventQueue *accelerometerEventQueue;
    ALooper *looper;

    GLuint shaderProgram;
    GLuint vPositionXHandle;
    GLuint vSensorValueHandle;
    GLuint uFragColorHandle;
    GLfloat xPos[SENSOR_HISTORY_LENGTH];

    struct AccelerometerData {
        GLfloat x;
        GLfloat y;
        GLfloat z;
    };
    AccelerometerData sensorData[SENSOR_HISTORY_LENGTH*2];
    AccelerometerData sensorDataFilter;
    int sensorDataIndex;

public:
    SensorGraphDemo() : sensorDataIndex(0) {}

    void init(AAssetManager *assetManager) {
        AAsset *vertexShaderAsset = AAssetManager_open(assetManager, "shader.glslv", AASSET_MODE_BUFFER);
        assert(vertexShaderAsset != NULL);
        const void *vertexShaderBuf = AAsset_getBuffer(vertexShaderAsset);
        assert(vertexShaderBuf != NULL);
        int vertexShaderLength = AAsset_getLength(vertexShaderAsset);
        vertexShaderSource = std::string((const char*)vertexShaderBuf, vertexShaderLength);
        AAsset_close(vertexShaderAsset);

        AAsset *fragmentShaderAsset = AAssetManager_open(assetManager, "shader.glslf", AASSET_MODE_BUFFER);
        assert(fragmentShaderAsset != NULL);
        const void *fragmentShaderBuf = AAsset_getBuffer(fragmentShaderAsset);
        assert(fragmentShaderBuf != NULL);
        int fragmentShaderLength = AAsset_getLength(fragmentShaderAsset);
        fragmentShaderSource = std::string((const char*)fragmentShaderBuf, fragmentShaderLength);
        AAsset_close(fragmentShaderAsset);

        sensorManager = ASensorManager_getInstance();
        assert(sensorManager != NULL);
        accelerometer = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ACCELEROMETER);
        assert(accelerometer != NULL);
        looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
        assert(looper != NULL);
        accelerometerEventQueue = ASensorManager_createEventQueue(sensorManager, looper, LOOPER_ID_USER, NULL, NULL);
        assert(accelerometerEventQueue != NULL);
        int setEventRateResult = ASensorEventQueue_setEventRate(accelerometerEventQueue,
                                                                accelerometer,
                                                                int32_t(1000000 /
                                                                        SENSOR_REFRESH_RATE));
        int enableSensorResult = ASensorEventQueue_enableSensor(accelerometerEventQueue, accelerometer);
        assert(enableSensorResult >= 0);
    }
    void surfaceChanged(int w, int h) {
        shaderProgram = createProgram(vertexShaderSource, fragmentShaderSource);
        assert(shaderProgram != 0);
        vPositionXHandle = glGetAttribLocation(shaderProgram, "vPositionX");
        vSensorValueHandle = glGetAttribLocation(shaderProgram, "vSensorValue");
        uFragColorHandle = glGetUniformLocation(shaderProgram, "uFragColor");

        glViewport(0, 0, w, h);
        generateXPos();
    }

    void generateXPos() {
        for (auto i = 0; i < SENSOR_HISTORY_LENGTH; i++) {
            float t = float(i) / float(SENSOR_HISTORY_LENGTH - 1);
            xPos[i] = -1.f * (1.f - t) + 1.f * t;
        }
    }

    GLuint createProgram(const std::string& pVertexSource, const std::string& pFragmentSource) {
        GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
        GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
        GLuint program = glCreateProgram();
        assert(program != 0);
        glAttachShader(program, vertexShader);
        glAttachShader(program, pixelShader);
        glLinkProgram(program);
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        assert(linked != 0);
        return program;
    }

    GLuint loadShader(GLenum shaderType, const std::string& pSource) {
        GLuint shader = glCreateShader(shaderType);
        assert(shader != 0);
        const char *sourceBuf = pSource.c_str();
        glShaderSource(shader, 1, &sourceBuf, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        assert(compiled != 0);
        return shader;
    }

    void update() {
        ALooper_pollAll(0, NULL, NULL, NULL);
        ASensorEvent event;
        float a = SENSOR_FILTER_ALPHA;
        while (ASensorEventQueue_getEvents(accelerometerEventQueue, &event, 1) > 0) {
            sensorDataFilter.x = a * event.acceleration.x + (1.0f - a) * sensorDataFilter.x;
            sensorDataFilter.y = a * event.acceleration.y + (1.0f - a) * sensorDataFilter.y;
            sensorDataFilter.z = a * event.acceleration.z + (1.0f - a) * sensorDataFilter.z;
        }
        sensorData[sensorDataIndex] = sensorData[SENSOR_HISTORY_LENGTH+sensorDataIndex] = sensorDataFilter;
        sensorDataIndex = (sensorDataIndex + 1) % SENSOR_HISTORY_LENGTH;
    }

    void render() {
        glClearColor(0.f, 0.f, 0.f, 1.0f);
        glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glEnableVertexAttribArray(vPositionXHandle);
        glVertexAttribPointer(vPositionXHandle, 1, GL_FLOAT, GL_FALSE, 0, xPos);

        glEnableVertexAttribArray(vSensorValueHandle);
        glVertexAttribPointer(vSensorValueHandle, 1, GL_FLOAT, GL_FALSE, sizeof(AccelerometerData), &sensorData[sensorDataIndex].x);

        glUniform4f(uFragColorHandle, 1.0f, 1.0f, 0.0f, 1.0f);
        glDrawArrays(GL_LINE_STRIP, 0, SENSOR_HISTORY_LENGTH);

        glVertexAttribPointer(vSensorValueHandle, 1, GL_FLOAT, GL_FALSE, sizeof(AccelerometerData), &sensorData[sensorDataIndex].y);
        glUniform4f(uFragColorHandle, 1.0f, 0.0f, 1.0f, 1.0f);
        glDrawArrays(GL_LINE_STRIP, 0, SENSOR_HISTORY_LENGTH);

        glVertexAttribPointer(vSensorValueHandle, 1, GL_FLOAT, GL_FALSE, sizeof(AccelerometerData), &sensorData[sensorDataIndex].z);
        glUniform4f(uFragColorHandle, 0.0f, 1.0f, 1.0f, 1.0f);
        glDrawArrays(GL_LINE_STRIP, 0, SENSOR_HISTORY_LENGTH);
    }

    void pause() {
        ASensorEventQueue_disableSensor(accelerometerEventQueue, accelerometer);
    }

    void resume() {
        ASensorEventQueue_enableSensor(accelerometerEventQueue, accelerometer);
    }
};

SensorGraphDemo gSensorGraphDemo;

extern "C" {
JNIEXPORT void JNICALL
    Java_com_android_gl2jni_GL2JNILib_init(JNIEnv *env, jclass type, jobject assetManager) {
        AAssetManager *nativeAssetManager = AAssetManager_fromJava(env, assetManager);
        gSensorGraphDemo.init(nativeAssetManager);
    }

    JNIEXPORT void JNICALL
    Java_com_android_gl2jni_GL2JNILib_surfaceChanged(JNIEnv *env, jclass type, jint width,
                                                     jint height) {
        gSensorGraphDemo.surfaceChanged(width, height);
    }


    JNIEXPORT void JNICALL
    Java_com_android_gl2jni_GL2JNILib_drawFrame(JNIEnv *env, jclass type) {
        gSensorGraphDemo.update();
        gSensorGraphDemo.render();
    }

    JNIEXPORT void JNICALL
    Java_com_android_gl2jni_GL2JNILib_pause(JNIEnv *env, jclass type) {
        gSensorGraphDemo.pause();
    }

    JNIEXPORT void JNICALL
    Java_com_android_gl2jni_GL2JNILib_resume(JNIEnv *env, jclass type) {
        gSensorGraphDemo.resume();
    }
}