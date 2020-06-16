//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLSurfaceTest:
//   Tests pertaining to egl::Surface.
//

#include <gtest/gtest.h>

#include <vector>

#include "common/Color.h"
#include "common/platform.h"
#include "test_utils/ANGLETest.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/Timer.h"

using namespace angle;

namespace
{

// A class to test various Android pre-rotation cases.  In order to make it easier to debug test
// failures, the initial window size is 256x256, and each pixel will have a unique and predictable
// value.  The red channel will increment with the x axis, and the green channel will increment
// with the y axis.  The four corners will have the following values:
//
// Where                 GLES Render &  ReadPixels coords       Color    (in Hex)
// Lower-left,  which is (-1.0,-1.0) & (  0,   0) in GLES will be black  (0x00, 0x00, 0x00, 0xFF)
// Lower-right, which is ( 1.0,-1.0) & (256,   0) in GLES will be red    (0xFF, 0x00, 0x00, 0xFF)
// Upper-left,  which is (-1.0, 1.0) & (  0, 256) in GLES will be green  (0x00, 0xFF, 0x00, 0xFF)
// Upper-right, which is ( 1.0, 1.0) & (256, 256) in GLES will be yellow (0xFF, 0xFF, 0x00, 0xFF)
class EGLPreRotationSurfaceTest : public ANGLETest
{
  protected:
    EGLPreRotationSurfaceTest()
        : mDisplay(EGL_NO_DISPLAY),
          mWindowSurface(EGL_NO_SURFACE),
          mContext(EGL_NO_CONTEXT),
          mOSWindow(nullptr),
          mSize(256)
    {}

    // Release any resources created in the test body
    void testTearDown() override
    {
        if (mDisplay != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (mWindowSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(mDisplay, mWindowSurface);
                mWindowSurface = EGL_NO_SURFACE;
            }

            if (mContext != EGL_NO_CONTEXT)
            {
                eglDestroyContext(mDisplay, mContext);
                mContext = EGL_NO_CONTEXT;
            }

            eglTerminate(mDisplay);
            mDisplay = EGL_NO_DISPLAY;
        }

        mOSWindow->destroy();
        OSWindow::Delete(&mOSWindow);

        ASSERT_TRUE(mWindowSurface == EGL_NO_SURFACE && mContext == EGL_NO_CONTEXT);
    }

    void testSetUp() override
    {
        mOSWindow = OSWindow::New();
        mOSWindow->initialize("EGLSurfaceTest", mSize, mSize);
    }

    void initializeDisplay()
    {
        GLenum platformType = GetParam().getRenderer();
        GLenum deviceType   = GetParam().getDeviceType();

        std::vector<EGLint> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(platformType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(deviceType);
        displayAttributes.push_back(EGL_NONE);

        mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                            reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                            displayAttributes.data());
        ASSERT_TRUE(mDisplay != EGL_NO_DISPLAY);

        EGLint majorVersion, minorVersion;
        ASSERT_TRUE(eglInitialize(mDisplay, &majorVersion, &minorVersion) == EGL_TRUE);

        eglBindAPI(EGL_OPENGL_ES_API);
        ASSERT_EGL_SUCCESS();
    }

    void initializeContext()
    {
        EGLint contextAttibutes[] = {EGL_CONTEXT_CLIENT_VERSION, GetParam().majorVersion, EGL_NONE};

        mContext = eglCreateContext(mDisplay, mConfig, nullptr, contextAttibutes);
        ASSERT_EGL_SUCCESS();
    }

    void initializeSurfaceWithRGBA8888Config()
    {
        const EGLint configAttributes[] = {
            EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8, EGL_BLUE_SIZE,      8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};

        EGLint configCount;
        EGLConfig config;
        ASSERT_TRUE(eglChooseConfig(mDisplay, configAttributes, &config, 1, &configCount) ||
                    (configCount != 1) == EGL_TRUE);

        mConfig = config;

        EGLint surfaceType = EGL_NONE;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_SURFACE_TYPE, &surfaceType);

        std::vector<EGLint> windowAttributes;
        windowAttributes.push_back(EGL_NONE);

        if (surfaceType & EGL_WINDOW_BIT)
        {
            // Create first window surface
            mWindowSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(),
                                                    windowAttributes.data());
            ASSERT_EGL_SUCCESS();
        }

        initializeContext();
    }

    void testDrawingAndReadPixels()
    {
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
        EXPECT_PIXEL_COLOR_EQ(0, mSize - 1, GLColor::green);
        EXPECT_PIXEL_COLOR_EQ(mSize - 1, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(mSize - 1, mSize - 1, GLColor::yellow);
        ASSERT_GL_NO_ERROR();

        eglSwapBuffers(mDisplay, mWindowSurface);
        ASSERT_EGL_SUCCESS();

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
        EXPECT_PIXEL_COLOR_EQ(0, mSize - 1, GLColor::green);
        EXPECT_PIXEL_COLOR_EQ(mSize - 1, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(mSize - 1, mSize - 1, GLColor::yellow);
        ASSERT_GL_NO_ERROR();

        {
            // Now, test a 4x4 area in the center of the window, which should tell us if a non-1x1
            // ReadPixels is oriented correctly for the device's orientation:
            GLint xOffset  = 126;
            GLint yOffset  = 126;
            GLsizei width  = 4;
            GLsizei height = 4;
            std::vector<GLColor> pixels(width * height);
            glReadPixels(xOffset, yOffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
            EXPECT_GL_NO_ERROR();
            // Expect that all red values equate to x and green values equate to y
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    int index = (y * width) + x;
                    GLColor expectedPixel(xOffset + x, yOffset + y, 0, 255);
                    GLColor actualPixel = pixels[index];
                    EXPECT_EQ(expectedPixel, actualPixel);
                }
            }
        }

        {
            // Now, test a 8x4 area off-the-center of the window, just to make sure that works too:
            GLint xOffset  = 13;
            GLint yOffset  = 26;
            GLsizei width  = 8;
            GLsizei height = 4;
            std::vector<GLColor> pixels2(width * height);
            glReadPixels(xOffset, yOffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &pixels2[0]);
            EXPECT_GL_NO_ERROR();
            // Expect that all red values equate to x and green values equate to y
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    int index = (y * width) + x;
                    GLColor expectedPixel(xOffset + x, yOffset + y, 0, 255);
                    GLColor actualPixel = pixels2[index];
                    EXPECT_EQ(expectedPixel, actualPixel);
                }
            }
        }

        eglSwapBuffers(mDisplay, mWindowSurface);
        ASSERT_EGL_SUCCESS();
    }

    EGLDisplay mDisplay;
    EGLSurface mWindowSurface;
    EGLContext mContext;
    EGLConfig mConfig;
    OSWindow *mOSWindow;
    int mSize;
};

// Provide a predictable pattern for testing pre-rotation
TEST_P(EGLPreRotationSurfaceTest, OrientedWindowWithDraw)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());

    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // To aid in debugging, we want this window visible
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithRGBA8888Config();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Init program
    constexpr char kVS[] =
        "attribute vec2 position;\n"
        "attribute vec2 redGreen;\n"
        "varying vec2 v_data;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "  v_data = redGreen;\n"
        "}";

    constexpr char kFS[] =
        "varying highp vec2 v_data;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(v_data, 0, 1);\n"
        "}";

    GLuint program = CompileProgram(kVS, kFS);
    ASSERT_NE(0u, program);
    glUseProgram(program);

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    GLint redGreenLocation = glGetAttribLocation(program, "redGreen");
    ASSERT_NE(-1, redGreenLocation);

    GLuint indexBuffer;
    glGenBuffers(1, &indexBuffer);

    GLuint vertexArray;
    glGenVertexArrays(1, &vertexArray);

    std::vector<GLuint> vertexBuffers(2);
    glGenBuffers(2, &vertexBuffers[0]);

    glBindVertexArray(vertexArray);

    std::vector<GLushort> indices = {0, 1, 2, 2, 3, 0};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), &indices[0],
                 GL_STATIC_DRAW);

    std::vector<GLfloat> positionData = {// quad vertices
                                         -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * positionData.size(), &positionData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(positionLocation);

    std::vector<GLfloat> redGreenData = {// green(0,1), black(0,0), red(1,0), yellow(1,1)
                                         0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * redGreenData.size(), &redGreenData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(redGreenLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(redGreenLocation);

    ASSERT_GL_NO_ERROR();

    testDrawingAndReadPixels();
}

// Use dFdx() and dFdy() and still provide a predictable pattern for testing pre-rotation
// In this case, the color values will be the following: (dFdx(v_data.x), dFdy(v_data.y), 0, 1).
// To help make this meaningful for pre-rotation, the derivatives will vary in the four corners of
// the window:
//
//  +------------+------------+      +--------+--------+
//  | (  0, 219) | (239, 249) |      | Green  | Yellow |
//  +------------+------------+  OR  +--------+--------+
//  | (  0,   0) | (229,   0) |      | Black  |  Red   |
//  +------------+------------+      +--------+--------+
TEST_P(EGLPreRotationSurfaceTest, OrientedWindowWithDerivativeDraw)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());

    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // To aid in debugging, we want this window visible
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithRGBA8888Config();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Init program
    constexpr char kVS[] =
        "#version 300 es\n"
        "in highp vec2 position;\n"
        "in highp vec2 redGreen;\n"
        "out highp vec2 v_data;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "  v_data = redGreen;\n"
        "}";

    constexpr char kFS[] =
        "#version 300 es\n"
        "in highp vec2 v_data;\n"
        "out highp vec4 FragColor;\n"
        "void main() {\n"
        "  FragColor = vec4(dFdx(v_data.x), dFdy(v_data.y), 0, 1);\n"
        "}";

    GLuint program = CompileProgram(kVS, kFS);
    ASSERT_NE(0u, program);
    glUseProgram(program);

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    GLint redGreenLocation = glGetAttribLocation(program, "redGreen");
    ASSERT_NE(-1, redGreenLocation);

    GLuint indexBuffer;
    glGenBuffers(1, &indexBuffer);

    GLuint vertexArray;
    glGenVertexArrays(1, &vertexArray);

    std::vector<GLuint> vertexBuffers(2);
    glGenBuffers(2, &vertexBuffers[0]);

    glBindVertexArray(vertexArray);

    std::vector<GLushort> indices = {// 4 squares each made up of 6 vertices:
                                     // 1st square, in the upper-left part of window
                                     0, 1, 2, 2, 3, 0,
                                     // 2nd square, in the upper-right part of window
                                     4, 5, 6, 6, 7, 4,
                                     // 3rd square, in the lower-left part of window
                                     8, 9, 10, 10, 11, 8,
                                     // 4th square, in the lower-right part of window
                                     12, 13, 14, 14, 15, 12};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), &indices[0],
                 GL_STATIC_DRAW);

    std::vector<GLfloat> positionData = {// 4 squares each made up of quad vertices
                                         // 1st square, in the upper-left part of window
                                         -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                         // 2nd square, in the upper-right part of window
                                         0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                                         // 3rd square, in the lower-left part of window
                                         -1.0f, 0.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
                                         // 4th square, in the lower-right part of window
                                         0.0f, 0.0f, 0.0f, -1.0f, 1.0f, -1.0f, 1.0f, 0.0f};

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * positionData.size(), &positionData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(positionLocation);

    std::vector<GLfloat> redGreenData = {// green(0,110), black(0,0), red(115,0), yellow(120,125)
                                         // 4 squares each made up of 4 pairs of half-color values:
                                         // 1st square, in the upper-left part of window
                                         0.0f, 110.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 110.0f,
                                         // 2nd square, in the upper-right part of window
                                         0.0f, 125.0f, 0.0f, 0.0f, 120.0f, 0.0f, 120.0f, 125.0f,
                                         // 3rd square, in the lower-left part of window
                                         0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         // 4th square, in the lower-right part of window
                                         0.0f, 0.0f, 0.0f, 0.0f, 115.0f, 0.0f, 115.0f, 0.0f};

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * redGreenData.size(), &redGreenData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(redGreenLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(redGreenLocation);

    ASSERT_GL_NO_ERROR();

    // Draw and check the 4 corner pixels, to ensure we're getting the expected "colors"
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
    GLColor expectedPixelLowerLeft(0, 0, 0, 255);
    GLColor expectedPixelLowerRight(229, 0, 0, 255);
    GLColor expectedPixelUpperLeft(0, 219, 0, 255);
    GLColor expectedPixelUpperRight(239, 249, 0, 255);
    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedPixelLowerLeft);
    EXPECT_PIXEL_COLOR_EQ(mSize - 1, 0, expectedPixelLowerRight);
    EXPECT_PIXEL_COLOR_EQ(0, mSize - 1, expectedPixelUpperLeft);
    EXPECT_PIXEL_COLOR_EQ(mSize - 1, mSize - 1, expectedPixelUpperRight);
    ASSERT_GL_NO_ERROR();

    // Make the image visible
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    // Draw again and check the 4 center pixels, to ensure we're getting the expected "colors"
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
    EXPECT_PIXEL_COLOR_EQ((mSize / 2) - 1, (mSize / 2) - 1, expectedPixelLowerLeft);
    EXPECT_PIXEL_COLOR_EQ((mSize / 2) - 1, (mSize / 2), expectedPixelUpperLeft);
    EXPECT_PIXEL_COLOR_EQ((mSize / 2), (mSize / 2) - 1, expectedPixelLowerRight);
    EXPECT_PIXEL_COLOR_EQ((mSize / 2), (mSize / 2), expectedPixelUpperRight);
    ASSERT_GL_NO_ERROR();
}

// A slight variation of EGLPreRotationSurfaceTest, where the initial window size is 400x300, yet
// the drawing is still 256x256.  In addition, gl_FragCoord is used in a "clever" way, as the color
// of the 256x256 drawing area, which reproduces an interesting pre-rotation case from the
// following dEQP tests:
//
// - dEQP.GLES31/functional_texture_multisample_samples_*_sample_position
//
// This will test the rotation of gl_FragCoord, as well as the viewport, scissor, and rendering
// area calculations, especially when the Android device is rotated.
class EGLPreRotationLargeSurfaceTest : public EGLPreRotationSurfaceTest
{
  protected:
    EGLPreRotationLargeSurfaceTest() : mSize(256) {}

    void testSetUp() override
    {
        mOSWindow = OSWindow::New();
        mOSWindow->initialize("EGLSurfaceTest", 400, 300);
    }

    int mSize;
};

// Provide a predictable pattern for testing pre-rotation
TEST_P(EGLPreRotationLargeSurfaceTest, OrientedWindowWithFragCoordDraw)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());

    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // To aid in debugging, we want this window visible
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithRGBA8888Config();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Init program
    constexpr char kVS[] =
        "attribute vec2 position;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "}";

    constexpr char kFS[] =
        "void main() {\n"
        "  gl_FragColor = vec4(gl_FragCoord.x / 256.0, gl_FragCoord.y / 256.0, 0.0, 1.0);\n"
        "}";

    GLuint program = CompileProgram(kVS, kFS);
    ASSERT_NE(0u, program);
    glUseProgram(program);

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    GLuint indexBuffer;
    glGenBuffers(1, &indexBuffer);

    GLuint vertexArray;
    glGenVertexArrays(1, &vertexArray);

    GLuint vertexBuffer;
    glGenBuffers(1, &vertexBuffer);

    glBindVertexArray(vertexArray);

    std::vector<GLushort> indices = {0, 1, 2, 2, 3, 0};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), &indices[0],
                 GL_STATIC_DRAW);

    std::vector<GLfloat> positionData = {// quad vertices
                                         -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * positionData.size(), &positionData[0],
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2, nullptr);
    glEnableVertexAttribArray(positionLocation);

    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, mSize, mSize);

    testDrawingAndReadPixels();
}

// Pre-rotation tests for glBlitFramebuffer.  A slight variation of EGLPreRotationLargeSurfaceTest,
// where the initial window size is still 400x300, and the drawing is still 256x256.  In addition,
// glBlitFramebuffer is tested in a variety of ways.  Separate tests are used to make debugging
// simpler, but they all share common setup.  These tests reproduce interesting pre-rotation cases
// from dEQP tests such as the following:
//
// - dEQP.GLES3/functional_fbo_blit_default_framebuffer_*
// - dEQP.GLES3/functional_fbo_invalidate_*
constexpr GLuint kCoordMidWayShort       = 127;
constexpr GLuint kCoordMidWayLong        = 128;
constexpr GLColor kColorMidWayShortShort = GLColor(127, 127, 0, 255);
constexpr GLColor kColorMidWayShortLong  = GLColor(127, 128, 0, 255);
constexpr GLColor kColorMidWayLongShort  = GLColor(128, 127, 0, 255);
constexpr GLColor kColorMidWayLongLong   = GLColor(128, 128, 0, 255);
// When scaling horizontally, the "black" and "green" colors have a 1 in the red component
constexpr GLColor kColorScaleHorizBlack = GLColor(1, 0, 0, 255);
constexpr GLColor kColorScaleHorizGreen = GLColor(1, 255, 0, 255);
// When scaling vertically, the "black" and "red" colors have a 1 in the green component
constexpr GLColor kColorScaleVertBlack = GLColor(0, 1, 0, 255);
constexpr GLColor kColorScaleVertRed   = GLColor(255, 1, 0, 255);

class EGLPreRotationBlitFramebufferTest : public EGLPreRotationLargeSurfaceTest
{
  protected:
    EGLPreRotationBlitFramebufferTest() {}

    GLuint createProgram()
    {
        constexpr char kVS[] =
            "attribute vec2 position;\n"
            "attribute vec2 redGreen;\n"
            "varying vec2 v_data;\n"
            "void main() {\n"
            "  gl_Position = vec4(position, 0, 1);\n"
            "  v_data = redGreen;\n"
            "}";

        constexpr char kFS[] =
            "varying highp vec2 v_data;\n"
            "void main() {\n"
            "  gl_FragColor = vec4(v_data, 0, 1);\n"
            "}";

        return CompileProgram(kVS, kFS);
    }

    void initializeGeometry(GLuint program)
    {
        GLint positionLocation = glGetAttribLocation(program, "position");
        ASSERT_NE(-1, positionLocation);

        GLint redGreenLocation = glGetAttribLocation(program, "redGreen");
        ASSERT_NE(-1, redGreenLocation);

        GLuint indexBuffer;
        glGenBuffers(1, &indexBuffer);

        GLuint vertexArray;
        glGenVertexArrays(1, &vertexArray);

        std::vector<GLuint> vertexBuffers(2);
        glGenBuffers(2, &vertexBuffers[0]);

        glBindVertexArray(vertexArray);

        std::vector<GLushort> indices = {0, 1, 2, 2, 3, 0};
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), &indices[0],
                     GL_STATIC_DRAW);

        std::vector<GLfloat> positionData = {// quad vertices
                                             -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * positionData.size(), &positionData[0],
                     GL_STATIC_DRAW);
        glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2,
                              nullptr);
        glEnableVertexAttribArray(positionLocation);

        std::vector<GLfloat> redGreenData = {// green(0,1), black(0,0), red(1,0), yellow(1,1)
                                             0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};

        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * redGreenData.size(), &redGreenData[0],
                     GL_STATIC_DRAW);
        glVertexAttribPointer(redGreenLocation, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2,
                              nullptr);
        glEnableVertexAttribArray(redGreenLocation);
    }

    GLuint createFBO()
    {
        GLuint framebuffer = 0;
        GLuint texture     = 0;
        glGenFramebuffers(1, &framebuffer);
        glGenTextures(1, &texture);

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mSize, mSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        return framebuffer;
    }

    // Ensures that the correct colors are where they should be when the entire 256x256 pattern has
    // been rendered or blitted to a location relative to an x and y offset.
    void test256x256PredictablePattern(GLint xOffset, GLint yOffset)
    {
        EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + 0, GLColor::black);
        EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + mSize - 1, GLColor::green);
        EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + mSize - 1, GLColor::yellow);
        EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort, yOffset + kCoordMidWayShort,
                              kColorMidWayShortShort);
        EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort, yOffset + kCoordMidWayLong,
                              kColorMidWayShortLong);
        EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayLong, yOffset + kCoordMidWayShort,
                              kColorMidWayLongShort);
        EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayLong, yOffset + kCoordMidWayLong,
                              kColorMidWayLongLong);
    }
};

// Draw a predictable pattern (for testing pre-rotation) into an FBO, and then use glBlitFramebuffer
// to blit that pattern into various places within the 400x300 window
TEST_P(EGLPreRotationBlitFramebufferTest, BasicBlitFramebuffer)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());

    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // To aid in debugging, we want this window visible
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithRGBA8888Config();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Init program
    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    glUseProgram(program);

    initializeGeometry(program);
    ASSERT_GL_NO_ERROR();

    // Create a texture-backed FBO and render the predictable pattern to it
    GLuint fbo = createFBO();
    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, mSize, mSize);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    ASSERT_GL_NO_ERROR();

    // Ensure the predictable pattern seems correct in the FBO
    test256x256PredictablePattern(0, 0);
    ASSERT_GL_NO_ERROR();

    //
    // Test blitting the entire FBO image to a 256x256 part of the default framebuffer (no scaling)
    //

    // Blit from the FBO to the default framebuffer (i.e. the swapchain)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize, 0, 0, mSize, mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();

    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize, 0, 0, mSize, mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    test256x256PredictablePattern(0, 0);
    ASSERT_GL_NO_ERROR();

    // Clear to black and blit to a different part of the window
    glClear(GL_COLOR_BUFFER_BIT);
    GLint xOffset = 40;
    GLint yOffset = 30;
    glViewport(xOffset, yOffset, mSize, mSize);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset, xOffset + mSize, yOffset + mSize,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();

    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset, xOffset + mSize, yOffset + mSize,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    test256x256PredictablePattern(xOffset, yOffset);
    ASSERT_GL_NO_ERROR();

    ASSERT_EGL_SUCCESS();
}

// Draw a predictable pattern (for testing pre-rotation) into an FBO, and then use glBlitFramebuffer
// to blit the left and right halves of that pattern into various places within the 400x300 window
TEST_P(EGLPreRotationBlitFramebufferTest, LeftAndRightBlitFramebuffer)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());

    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // To aid in debugging, we want this window visible
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithRGBA8888Config();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Init program
    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    glUseProgram(program);

    initializeGeometry(program);
    ASSERT_GL_NO_ERROR();

    // Create a texture-backed FBO and render the predictable pattern to it
    GLuint fbo = createFBO();
    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, mSize, mSize);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    ASSERT_GL_NO_ERROR();

    // Ensure the predictable pattern seems correct in the FBO
    test256x256PredictablePattern(0, 0);
    ASSERT_GL_NO_ERROR();

    // Prepare to blit to the default framebuffer and read from the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Blit to an offset part of the 400x300 window
    GLint xOffset = 40;
    GLint yOffset = 30;

    //
    // Test blitting half of the FBO image to a 128x256 or 256x128 part of the default framebuffer
    // (no scaling)
    //

    // 1st) Clear to black and blit the left and right halves of the texture to the left and right
    // halves of that different part of the window
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(xOffset, yOffset, mSize, mSize);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, mSize / 2, mSize, xOffset, yOffset, xOffset + (mSize / 2),
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(mSize / 2, 0, mSize, mSize, xOffset + (mSize / 2), yOffset, xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();

    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize / 2, mSize, xOffset, yOffset, xOffset + (mSize / 2),
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(mSize / 2, 0, mSize, mSize, xOffset + (mSize / 2), yOffset, xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    test256x256PredictablePattern(xOffset, yOffset);
    ASSERT_GL_NO_ERROR();

    // 2nd) Clear to black and this time blit the left half of the source texture to the right half
    // of the destination window, and then blit the right half of the source texture to the left
    // half of the destination window
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(xOffset, yOffset, mSize, mSize);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(mSize / 2, 0, mSize, mSize, xOffset, yOffset, xOffset + (mSize / 2),
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, mSize / 2, mSize, xOffset + (mSize / 2), yOffset, xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();

    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(mSize / 2, 0, mSize, mSize, xOffset, yOffset, xOffset + (mSize / 2),
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, mSize / 2, mSize, xOffset + (mSize / 2), yOffset, xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort + 1, yOffset + 0, GLColor::black);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort + 1, yOffset + mSize - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort, yOffset + 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort, yOffset + mSize - 1, GLColor::yellow);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + kCoordMidWayShort, kColorMidWayShortShort);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + kCoordMidWayLong, kColorMidWayShortLong);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + kCoordMidWayShort, kColorMidWayLongShort);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + kCoordMidWayLong, kColorMidWayLongLong);
    ASSERT_GL_NO_ERROR();

    ASSERT_EGL_SUCCESS();
}

// Draw a predictable pattern (for testing pre-rotation) into an FBO, and then use glBlitFramebuffer
// to blit the top and bottom halves of that pattern into various places within the 400x300 window
TEST_P(EGLPreRotationBlitFramebufferTest, TopAndBottomBlitFramebuffer)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());

    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // To aid in debugging, we want this window visible
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithRGBA8888Config();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Init program
    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    glUseProgram(program);

    initializeGeometry(program);
    ASSERT_GL_NO_ERROR();

    // Create a texture-backed FBO and render the predictable pattern to it
    GLuint fbo = createFBO();
    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, mSize, mSize);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    ASSERT_GL_NO_ERROR();

    // Ensure the predictable pattern seems correct in the FBO
    test256x256PredictablePattern(0, 0);
    ASSERT_GL_NO_ERROR();

    // Prepare to blit to the default framebuffer and read from the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Blit to an offset part of the 400x300 window
    GLint xOffset = 40;
    GLint yOffset = 30;

    //
    // Test blitting half of the FBO image to a 128x256 or 256x128 part of the default framebuffer
    // (no scaling)
    //

    // 1st) Clear to black and blit the top and bottom halves of the texture to the top and bottom
    // halves of that different part of the window
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(xOffset, yOffset, mSize, mSize);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, mSize, mSize / 2, xOffset, yOffset, xOffset + mSize,
                      yOffset + (mSize / 2), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, mSize / 2, mSize, mSize, xOffset, yOffset + (mSize / 2), xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();

    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize / 2, xOffset, yOffset, xOffset + mSize,
                      yOffset + (mSize / 2), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, mSize / 2, mSize, mSize, xOffset, yOffset + (mSize / 2), xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    test256x256PredictablePattern(xOffset, yOffset);
    ASSERT_GL_NO_ERROR();

    // 2nd) Clear to black and this time blit the top half of the source texture to the bottom half
    // of the destination window, and then blit the bottom half of the source texture to the top
    // half of the destination window
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(xOffset, yOffset, mSize, mSize);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, mSize, mSize / 2, xOffset, yOffset + (mSize / 2), xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, mSize / 2, mSize, mSize, xOffset, yOffset, xOffset + mSize,
                      yOffset + (mSize / 2), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();

    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize / 2, xOffset, yOffset + (mSize / 2), xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, mSize / 2, mSize, mSize, xOffset, yOffset, xOffset + mSize,
                      yOffset + (mSize / 2), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + kCoordMidWayShort + 1, GLColor::black);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + kCoordMidWayShort, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + kCoordMidWayShort + 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + kCoordMidWayShort, GLColor::yellow);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort, yOffset + mSize - 1, kColorMidWayShortShort);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort, yOffset + 0, kColorMidWayShortLong);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayLong, yOffset + mSize - 1, kColorMidWayLongShort);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayLong, yOffset + 0, kColorMidWayLongLong);
    ASSERT_GL_NO_ERROR();

    ASSERT_EGL_SUCCESS();
}

// Draw a predictable pattern (for testing pre-rotation) into an FBO, and then use glBlitFramebuffer
// to blit that pattern into various places within the 400x300 window, but being scaled to one-half
// size
TEST_P(EGLPreRotationBlitFramebufferTest, ScaledBlitFramebuffer)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());

    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // To aid in debugging, we want this window visible
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithRGBA8888Config();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Init program
    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    glUseProgram(program);

    initializeGeometry(program);
    ASSERT_GL_NO_ERROR();

    // Create a texture-backed FBO and render the predictable pattern to it
    GLuint fbo = createFBO();
    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, mSize, mSize);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    ASSERT_GL_NO_ERROR();

    // Ensure the predictable pattern seems correct in the FBO
    test256x256PredictablePattern(0, 0);
    ASSERT_GL_NO_ERROR();

    // Prepare to blit to the default framebuffer and read from the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Blit to an offset part of the 400x300 window
    GLint xOffset = 40;
    GLint yOffset = 30;

    //
    // Test blitting the entire FBO image to a 128x256 or 256x128 part of the default framebuffer
    // (requires scaling)
    //

    // 1st) Clear to black and blit the FBO to the left and right halves of that different part of
    // the window
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(xOffset, yOffset, mSize, mSize);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset, xOffset + (mSize / 2), yOffset + mSize,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset + (mSize / 2), yOffset, xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();

    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset, xOffset + (mSize / 2), yOffset + mSize,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset + (mSize / 2), yOffset, xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + 0, kColorScaleHorizBlack);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + mSize - 1, kColorScaleHorizGreen);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort, yOffset + 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayShort, yOffset + mSize - 1, GLColor::yellow);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayLong, yOffset + 0, kColorScaleHorizBlack);
    EXPECT_PIXEL_COLOR_EQ(xOffset + kCoordMidWayLong, yOffset + mSize - 1, kColorScaleHorizGreen);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + mSize - 1, GLColor::yellow);

    // 2nd) Clear to black and blit the FBO to the top and bottom halves of that different part of
    // the window
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(xOffset, yOffset, mSize, mSize);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset, xOffset + mSize, yOffset + (mSize / 2),
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset + (mSize / 2), xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();

    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset, xOffset + mSize, yOffset + (mSize / 2),
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset + (mSize / 2), xOffset + mSize,
                      yOffset + mSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + 0, kColorScaleVertBlack);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + 0, kColorScaleVertRed);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + kCoordMidWayShort, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + kCoordMidWayShort, GLColor::yellow);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + kCoordMidWayLong, kColorScaleVertBlack);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + kCoordMidWayLong, kColorScaleVertRed);
    EXPECT_PIXEL_COLOR_EQ(xOffset + 0, yOffset + mSize - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(xOffset + mSize - 1, yOffset + mSize - 1, GLColor::yellow);
    ASSERT_GL_NO_ERROR();

    ASSERT_EGL_SUCCESS();
}

// Draw a predictable pattern (for testing pre-rotation) into a 256x256 portion of the 400x300
// window, and then use glBlitFramebuffer to blit that pattern into an FBO
TEST_P(EGLPreRotationBlitFramebufferTest, FboDestBlitFramebuffer)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());

    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());

    // To aid in debugging, we want this window visible
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithRGBA8888Config();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Init program
    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    glUseProgram(program);

    initializeGeometry(program);
    ASSERT_GL_NO_ERROR();

    // Create a texture-backed FBO and render the predictable pattern to it
    GLuint fbo = createFBO();
    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, mSize, mSize);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    ASSERT_GL_NO_ERROR();

    // Ensure the predictable pattern seems correct in the FBO
    test256x256PredictablePattern(0, 0);
    ASSERT_GL_NO_ERROR();

    // Prepare to blit to the default framebuffer and read from the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Blit to an offset part of the 400x300 window
    GLint xOffset = 40;
    GLint yOffset = 30;

    //
    // Test blitting a 256x256 part of the default framebuffer to the entire FBO (no scaling)
    //

    // To get the entire predictable pattern into the default framebuffer at the desired offset,
    // blit it from the FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(xOffset, yOffset, mSize, mSize);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset, xOffset + mSize, yOffset + mSize,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    // Swap buffers to put the image in the window (so the test can be visually checked)
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_GL_NO_ERROR();
    // Blit again to check the colors in the back buffer
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, mSize, mSize, xOffset, yOffset, xOffset + mSize, yOffset + mSize,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Clear the FBO to black and blit from the window to the FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glViewport(0, 0, mSize, mSize);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(xOffset, yOffset, xOffset + mSize, yOffset + mSize, 0, 0, mSize, mSize,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Ensure the predictable pattern seems correct in the FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    test256x256PredictablePattern(0, 0);
    ASSERT_GL_NO_ERROR();

    ASSERT_EGL_SUCCESS();
}

}  // anonymous namespace

ANGLE_INSTANTIATE_TEST(EGLPreRotationSurfaceTest,
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));
ANGLE_INSTANTIATE_TEST(EGLPreRotationLargeSurfaceTest,
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));
ANGLE_INSTANTIATE_TEST(EGLPreRotationBlitFramebufferTest,
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));
