#include "image_processing.h"
#include <JPEGDecoder.h>
#include "esp_camera.h"


constexpr int height_orig = 96;
constexpr int width_orig = 96;
constexpr int height_new = 28;
constexpr int width_new = 28;
constexpr int height_temp = 20;
constexpr int width_temp = 20;
int indexY = 0;
int indexX = 0;
uint8_t array_temp[height_temp][width_temp] = {{0}};
uint8_t crop_dim[4] = {0};

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22



// Get the camera module ready
bool InitCamera() {
  Serial.println("In init");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;


  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  config.frame_size = FRAMESIZE_96X96;
  //config.jpeg_quality = 15;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.println("init failed");
    return false;
  }
  return true;
}


// Decode the JPEG image, crop it, and convert it to greyscale
bool DecodeAndCropImage(int image_width, int image_height,
                        uint8_t* image_data) {
  //uint8_t threshold = 180; // Day value
  uint8_t threshold = 230;
  int offset = 20;
  //offset = 17;
  int top = 0, bottom = 0, left = 0, right = 0;
  int index1 = 0;

  int k = 0;
  int newIndex = 0;
  int prevIndex = 0;
  int indexChange = 0;
  int temp = 0;
  String x;
  int tempValue;
  bool white_background = false;
  
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("fb failed");
    return false;
  }

  //If the background is white, invert the whole image
  if (white_background)
  {
    for (int j = 0; j < height_orig; j++)
    {
      for (int i = 0; i < width_orig; i++)
      {
        index1 = j * width_orig + i;
        fb->buf[index1] = 255 - fb->buf[index1];
      }
    }
  }

  //Get the top, bottom, left and right boundary of the image to create a bounding box
  for (int j = 0; j < height_orig; j++)
  {
    for (int i = 0; i < width_orig; i++)
    { index1 = j * width_orig + i;
      if (fb->buf[index1] >= threshold)
      {
        top = j;
        break;
      }


    }
    if (fb->buf[index1] >= threshold)
      break;
  }

  for (int j = height_orig - 1; j >= 0; j--)
  {
    for (int i = width_orig - 1; i >= 0; i--)
    { index1 = j * width_orig + i;
      if (fb->buf[index1] >= threshold)
      {
        bottom = j;
        break;
      }
    }
    if (fb->buf[index1] >= threshold)
      break;
  }

  for (int i = width_orig - 1; i >= 0; i--)
  {
    for (int j = height_orig - 1; j >= 0; j--)
    { index1 = i + j * width_orig;
      if (fb->buf[index1] >= threshold)
      {
        right = i;
        break;
      }
    }
    if (fb->buf[index1] >= threshold)
      break;
  }


  for (int i = 0; i < width_orig; i++)
  {
    for (int j = 0; j < height_orig; j++)
    { index1 = i + j * width_orig;
      if (fb->buf[index1] >= threshold)
      {
        left = i;
        break;
      }
    }
    if (fb->buf[index1] >= threshold)
      break;
  }

  int crop_height_orig = height_orig - top - (height_orig - bottom) + offset;
  int crop_width_orig = width_orig - left - (width_orig - right) + offset;


  //See if the boundaries are not out of bound for cropping, otherwise return with getting image
  if (bottom - top > 60)
  {

    if (bottom - top < 90)
    {
      crop_height_orig -= offset + 2; //Remove the offset in that case
    }
    else
    {
      esp_camera_fb_return(fb);
      return false;

    }

  }

  if (right - left > 70)
  {
    Serial.println(crop_height_orig);
    crop_width_orig -= offset + 1; //Remove the offset in that case
    esp_camera_fb_return(fb);
    return false;
  }

  if (right - left < 10)
  {
    if (right < 90)
      right += 5;

    if (left > 10)
      left -= 5;

  }

  uint8_t cropped_img [crop_height_orig][crop_width_orig] = {0};


  //Determine the number of pixels required to interpolate for resizing
  float yChange = (crop_height_orig / height_temp);
  float xChange = crop_width_orig / width_temp;

  //Init the array to zero
  for (int j = 0; j < crop_height_orig; j++)
  {
    for (int i = 0; i < crop_width_orig; i++)
    {
      cropped_img[j][i] = 0 ;
    }
  }
  int j2 = 0;

  //Copy the cropped image to this new image
  for (int j = top; j < bottom; j++)
  {
    int i2 = 0;
    for (int i = left; i < right; i++)
    {
      index1 = j * width_orig + i;
      if (fb->buf[index1] >= threshold)
        cropped_img[j2][i2] = fb->buf[index1];

      i2++;
    }
    j2++;
  }
  int prevY = indexY;
  int prevX = indexX;
  float temp_value = 0;
  int pixel_counter = 0;
  Serial.println("Before thresholding");
  for (int j = 0; j < height_temp; j++)
  {
    for (int i = 0; i < width_temp; i++)
    {
      prevY = indexY;
      prevX = indexX;
      //Get previous indexes

      //Calculate new indexes
      indexY = j * yChange;
      indexX = i * xChange;
      temp_value = 0;
      pixel_counter = 0;
      array_temp[j][i] = 0;
      //Loop over the whole gride to calculated average value
      for (int jj = 0; jj < yChange; jj++)
      {
        for (int ii = 0; ii < xChange; ii++)
        {
          if ((indexX + jj) < crop_height_orig && (prevX + ii) < crop_width_orig)
          {
            temp_value += cropped_img[prevY + jj][prevX + ii];
            pixel_counter++;
          }

        }
      }
      //Divde by number of pixels in the grid
      if (pixel_counter > 0)
        temp_value /= pixel_counter;//(xChange*yChange);

      array_temp[j][i] = temp_value;//cropped_img[indexY][indexX];
    }
  }


  //Copy the cropped 20x20 image into 28x28 and center it in the grid
  for (int j = 0; j < height_new; j++)
  {
    for (int i = 0; i < width_new; i++)
    { index1 = j * width_new + i;
      if (j > 3 && j < 24 && i > 3 && i < 24)
      {
        image_data[index1] = array_temp[j - 4][i - 4];
      }
      else
      {
        image_data[index1] = 0;
      }
    }
  }


  //Apply a final threshold in order to getting a better resolution image
  for (int j = 0; j < height_new; j++)
  {

    for (int i = 0; i < width_new; i++) //Should be 160
    {

      if (image_data[k] >= threshold)
        image_data[k] = 255;
      else
        image_data[k] = 0;
      x = String(image_data[k]) + ", ";
      Serial.print(x);
      k++;
    }
    Serial.println("");
  }
  Serial.println("]");

  esp_camera_fb_return(fb);
  return true;
}


// Get an image from the camera module
bool GetImage(int image_width, int image_height, int channels, uint8_t* image_data) {
  static bool g_is_camera_initialized = false;
  if (!g_is_camera_initialized) {
    bool init_status = InitCamera();
    if (init_status != true) {
      Serial.println("Init failed");
      return init_status;
    }
    g_is_camera_initialized = true;
  }
  bool decode_status = DecodeAndCropImage(image_width, image_height, image_data);
  if (decode_status != true) {
    Serial.println("Decode failed");
    return decode_status;
  }

  return true;
}
