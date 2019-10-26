#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <string>
#include <cerrno>
#include <vector>
#include "math.h"
#include "tiffio.h"

bool imread_tiff(const std::string input_image_filename, std::vector<float> &image, uint32_t &imageLengthOut, uint32_t &imageWidthOut)
{
    TIFFSetWarningHandler(NULL);
    TIFF* tif = TIFFOpen(input_image_filename.c_str(), "r");
    if (!tif)
	{
        std::cerr << "imread_tiff: Could not read input file!" << std::endl;
        return true;
	}
    uint32 imageLength;
	uint32 imageWidth;
    uint16 num_chan;
	uint16_t * buf16;
	unsigned char  * buf8;
	uint16 bits_per_sample;

	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imageLength);
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imageWidth);
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &num_chan);
	TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);

    std::cout << "length: " << imageLength << ". width: " << imageWidth << std::endl;

	image.resize(imageLength * imageWidth * 4);
    imageLengthOut = imageLength;
    imageWidthOut = imageWidth;

    //The matrix is 3x wider than the image, interleaving the channels.
	if(bits_per_sample == 16)
	{
	  buf16 = (uint16_t *)_TIFFmalloc(TIFFScanlineSize(tif));
      for ( unsigned int row = 0; row < imageLength; row++)
	  {
	      TIFFReadScanline(tif, buf16, row);
          for( unsigned int col = 0; col < imageWidth; col++)
		  {
            //   if (row == 0)
            //       std::cout << buf16[col*num_chan    ]  << std::endl;
			  image[row*imageWidth*4 + col*4    ] = buf16[col*num_chan    ];
			  image[row*imageWidth*4 + col*4 + 1] = buf16[col*num_chan + 1];
              image[row*imageWidth*4 + col*4 + 2] = buf16[col*num_chan + 2];
              image[row*imageWidth*4 + col*4 + 3] = 65535.0;
		  }
	  }
	  _TIFFfree(buf16);
	}
	else
	{
	  buf8 = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(tif));
      for ( unsigned int row = 0; row < imageLength; row++)
	  {
	      TIFFReadScanline(tif, buf8, row);
          for( unsigned int col = 0; col < imageWidth; col++)
		  {
			  image[row*imageWidth*4 + col*4    ] = float(buf8[col*num_chan    ])*257;
			  image[row*imageWidth*4 + col*4 + 1] = float(buf8[col*num_chan + 1])*257;
              image[row*imageWidth*4 + col*4 + 2] = float(buf8[col*num_chan + 2])*257;
              image[row*imageWidth*4 + col*4 + 3] = 65535.0;
		  }
	  }
	  _TIFFfree(buf8 );
	}
	TIFFClose(tif);

	return false;
}

std::string get_file_contents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  std::cerr << filename << " could not be read." << std::endl;
  throw(errno);
}

static int CompileShader(unsigned int type, const std::string& source)
{
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE)
    {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(length);
        glGetShaderInfoLog(id, length, &length, &message[0]);
        std::string shaderType = (type == GL_COMPUTE_SHADER) ? "compute" : (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        std::cout << "failed to compile " << shaderType << " shader!" << std::endl;
        std::cout << &message[0] << std::endl;
        glDeleteShader(id);
        return 0;
    }

    return id;
}

static int CreateDispProgram(const std::string& vertexShader,
                             const std::string& fragmentShader)
{
    unsigned int program = glCreateProgram();
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);

    return program;
}

static int CreateCompProgram(const std::string& computeShaderStr)
{
    std::cout << "Compiling compute shader" << std::endl;
    unsigned int compProgram = glCreateProgram();
    unsigned int cs = CompileShader(GL_COMPUTE_SHADER, computeShaderStr);
    glAttachShader(compProgram, cs);
    glLinkProgram(compProgram);
    int success;
    glGetProgramiv(compProgram, GL_LINK_STATUS, &success);
    if (success != GL_TRUE)
    {
        int length;
        glGetProgramiv(compProgram, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(length);
        glGetProgramInfoLog(compProgram, length, &length, &message[0]);
        std::cout << "failed to link shader!" <<  std::endl;
        std::cout << &message[0] << std::endl;
    }

    glValidateProgram(compProgram);
    glGetProgramiv(compProgram, GL_VALIDATE_STATUS, &success);
    if (success != GL_TRUE)
    {
        int length;
        glGetProgramiv(compProgram, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> message(length);
        glGetProgramInfoLog(compProgram, length, &length, &message[0]);
        std::cout << "failed to validate shader!" <<  std::endl;
        std::cout << &message[0] << std::endl;
    }

    return compProgram;
}

void GLAPIENTRY
MessageCallback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
  fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}

int main(void)
{  
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    if ( glewInit() != GLEW_OK ){
        std::cout << "Error with glewInit";
        return -1;
    }

    glEnable              ( GL_DEBUG_OUTPUT );
    glDebugMessageCallback( MessageCallback, 0 );

    float vertices[] = {
        -1.0f,-1.0f, 0.0f, 1.0f,
         1.0f,-1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, 0.0f,

        -1.0f,-1.0f, 0.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f,
         1.0f, 1.0f, 1.0f, 0.0f,
    };

    int numTimesteps = 12;

    GLuint vao;
    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);

    unsigned int buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    uint32_t texWidth = 4096, texHeight = 4096;
    std::vector<float> inputImageVector;
    imread_tiff("test.tiff", inputImageVector, texHeight, texWidth);
    std::cout << texHeight << " " << texWidth << std::endl;

    float borderColor[] = { 1.0f, 0.0f, 0.0f, 1.0f };

    GLuint activeCrystals;
    glGenTextures(1, &activeCrystals);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, activeCrystals);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texWidth, texHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, activeCrystals, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    // std::vector<float> developerConcCPU(texHeight*texWidth,0.0f);
    // developerConcCPU[(texHeight/2)*texWidth + texWidth/2] = pow(100*2*3.1415,numTimesteps);
    //developerConcCPU[texWidth/2] = 1.0;// pow(100*2*3.1415,numTimesteps);
    GLuint developer;
    glGenTextures(1, &developer);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, developer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texWidth, texHeight, 0, GL_RED, GL_FLOAT, NULL);//&developerConcCPU[0]);
    glBindImageTexture(1, developer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    GLuint crystalRadius;
    glGenTextures(1, &crystalRadius);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, crystalRadius);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texWidth, texHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(2, crystalRadius, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    

    GLuint silverSaltDensity;
    glGenTextures(1, &silverSaltDensity);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, silverSaltDensity);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texWidth, texHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(3, silverSaltDensity, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    struct pixel
    {
        float data[4];
    };
    
    // std::vector<pixel> pixels(texHeight*texWidth,{0.0f,0.0f,0.0f,0.0f});//{pow(2,12),pow(2,12),pow(2,12),1.0});
    // pixels[(texHeight/2)*texWidth + texWidth/2] = {pow(2,13),pow(2,13),pow(2,13),1.0};
    GLuint inputImage;
    glGenTextures(1, &inputImage);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, inputImage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texWidth, texHeight, 0, GL_RGBA, GL_FLOAT, &inputImageVector[0]);
    glBindImageTexture(4, inputImage, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    int smallTexWidth = texWidth/8;
    int smallTexHeight = texHeight/8;
    //std::vector<float> developerSmallConcCPU(smallTexHeight*smallTexWidth,0.0f);
    //developerSmallConcCPU[(smallTexHeight/2)*smallTexWidth + smallTexWidth/2] = 100;//pow(100*2*3.1415,numTimesteps);
    GLuint developerSmall;
    glGenTextures(1, &developerSmall);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, developerSmall);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, smallTexWidth, smallTexHeight, 0, GL_RED, GL_FLOAT,NULL);
    glBindImageTexture(5, developerSmall, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    std::string vertexShaderStr = get_file_contents("vertex.glsl");
    std::string fragmentShaderStr = get_file_contents("fragment.glsl");
    std::string blurYShaderStr = get_file_contents("blurYSmall.glsl");
    std::string blurXShaderStr = get_file_contents("blurXSmall.glsl");
    std::string devShaderStr = get_file_contents("develop.glsl");
    std::string expShaderStr = get_file_contents("exposure.glsl");

    unsigned int dispProgram = CreateDispProgram(vertexShaderStr, fragmentShaderStr);
    GLuint blurYProgram = CreateCompProgram(blurYShaderStr);
    GLuint blurXProgram = CreateCompProgram(blurXShaderStr);
    GLuint developProgram = CreateCompProgram(devShaderStr);
    GLuint exposureProgram = CreateCompProgram(expShaderStr);

    float sigma = 10;

    float q;
    if (sigma < 2.5)
    {
        q = 3.97156 - 4.14554*sqrt(1 - 0.26891*sigma);
    }
    else
    {
        q = 0.98711*sigma - 0.96330;
    }
    
    double denom = 1.57825 + 2.44413*q + 1.4281*q*q + 0.422205*q*q*q;

    const float coeff1 = float((2.44413*q + 2.85619*q*q + 1.26661*q*q*q)/denom);
    const float coeff2 = float((-1.4281*q*q - 1.26661*q*q*q)/denom);
    const float coeff3 = float((0.422205*q*q*q)/denom);
    const float coeff0 = float(1 - (coeff1 + coeff2 + coeff3));

    GLint coeffLocY = glGetUniformLocation(blurYProgram, "coeffs");
    GLint coeffLocX = glGetUniformLocation(blurXProgram, "coeffs");

    GLint crystalGrowthRateLoc = glGetUniformLocation(developProgram, "crystalGrowthRate");
    GLint developerConsumptionRateLoc = glGetUniformLocation(developProgram, "developerConsumptionRate");
    GLint silverSaltConsumptionRateLoc = glGetUniformLocation(developProgram, "silverSaltConsumptionRate");

    GLint sigmaXLoc = glGetUniformLocation(blurXProgram, "sigma");
    GLint sigmaYLoc = glGetUniformLocation(blurYProgram, "sigma");

    GLint mipLevelXLoc = glGetUniformLocation(blurXProgram, "mipLevel");
    GLint mipLevelYLoc = glGetUniformLocation(blurYProgram, "mipLevel");

    GLint rolloffBoundaryLoc = glGetUniformLocation(exposureProgram, "rolloffBoundary");


    GLint posAttrib = glGetAttribLocation(dispProgram, "position");
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0);
    glEnableVertexAttribArray(posAttrib);

    GLint uvAttrib = glGetAttribLocation(dispProgram, "uvIn");
    glVertexAttribPointer(uvAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(uvAttrib);

    bool firstLoop = true;
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        if(firstLoop)
        { // Compute Shader
            glfwSetTime(0);

            float initialCrystalRadius[3] = {0.00001, 0.00001, 0.00001};
            glClearTexImage(crystalRadius, 0, GL_RGB, GL_FLOAT, &initialCrystalRadius[0]);

            float initialSilverSaltDensity[3] = {1, 1, 1};
            glClearTexImage(silverSaltDensity, 0, GL_RGB, GL_FLOAT, &initialSilverSaltDensity[0]);

            float initialDeveloperConcentration = 1.0f;
            glActiveTexture(GL_TEXTURE5);
            glClearTexImage(developerSmall, 0, GL_RED, GL_FLOAT, &initialDeveloperConcentration);

            glMemoryBarrier(GL_ALL_BARRIER_BITS);

            //Expose
            glUseProgram(exposureProgram);

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, inputImage);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, activeCrystals);

            glUniform1f(rolloffBoundaryLoc, pow(2,16));

            glDispatchCompute((GLuint)ceil(float(texWidth)/64), (GLuint)ceil(float(texHeight)/8), 1);

            glMemoryBarrier(GL_ALL_BARRIER_BITS);

            for (int ii = 0; ii < numTimesteps; ii++)
            {
                //Develop
                glUseProgram(developProgram);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, activeCrystals);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, developer);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, crystalRadius);
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, silverSaltDensity);
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_2D, developerSmall);

                glUniform1f(crystalGrowthRateLoc, 0.00001 * 100.0/12.0);
                glUniform1f(developerConsumptionRateLoc, 2000000.0 * 2.0/(3.0*0.1));
                glUniform1f(silverSaltConsumptionRateLoc, 2000000.0 * 2.0);

                glDispatchCompute((GLuint)ceil(float(texWidth)/64), (GLuint)ceil(float(texHeight)/8), 1);

                glMemoryBarrier(GL_ALL_BARRIER_BITS);

                //Make mipmaps
                glBindTexture(GL_TEXTURE_2D, developer);
                glGenerateMipmap(GL_TEXTURE_2D);
                glMemoryBarrier(GL_ALL_BARRIER_BITS);

                //X minifying blur blur
                glUseProgram(blurXProgram);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, developer);

                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_2D, developerSmall);

                glUniform1f(sigmaXLoc,sigma*float(smallTexWidth)/texWidth);
                glUniform1ui(mipLevelXLoc,3);

                glDispatchCompute((GLuint)ceil(float(smallTexWidth)/64), (GLuint)ceil(float(smallTexHeight)/8), 1);

                glMemoryBarrier(GL_ALL_BARRIER_BITS);

                //Y Blur on small texture
                glUseProgram(blurYProgram);

                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_2D, developerSmall);

                glUniform1f(sigmaYLoc, sigma*float(smallTexWidth)/texWidth);
                glUniform1ui(mipLevelYLoc,3);

                glDispatchCompute((GLuint)ceil(float(smallTexWidth)/64), (GLuint)ceil(float(smallTexHeight)/8), 1);

                glMemoryBarrier(GL_ALL_BARRIER_BITS);
            }
            glFinish();
            std::cout << "filmulation took " << 1000*glfwGetTime() << " ms." << std::endl;
        }
        //firstLoop = false;

        {/* Render here */
            glClearColor(1.0f,0.0f,0.0f,1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(dispProgram);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, activeCrystals);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, developer);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, crystalRadius);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, silverSaltDensity);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, inputImage);
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, developerSmall);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glDeleteProgram(dispProgram);

    glfwTerminate();
    return 0;
}
