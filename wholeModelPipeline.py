import tensorflow as tf
import numpy as np
import gzip


# get images from the gzip file
def get_images(path):
    with gzip.open(path, 'r') as f:
        # first 4 bytes is a magic number
        magic_number = int.from_bytes(f.read(4), 'big')
        print(magic_number)
        # second 4 bytes is the number of images
        image_count = int.from_bytes(f.read(4), 'big')
        print(image_count)
        # third 4 bytes is the row count
        row_count = int.from_bytes(f.read(4), 'big')
        print(row_count)
        # fourth 4 bytes is the column count
        column_count = int.from_bytes(f.read(4), 'big')
        print(column_count)
        # rest is the image pixel data, each pixel is stored as an unsigned byte
        # pixel values are 0 to 255
        image_data = f.read()
        images = np.frombuffer(image_data, dtype=np.uint8) \
            .reshape((image_count, row_count, column_count))
        return images


# get labels from the gzip file
def get_labels(path):
    with gzip.open(path, 'r') as f:
        # first 4 bytes is a magic number
        magic_number = int.from_bytes(f.read(4), 'big')
        # second 4 bytes is the number of labels
        label_count = int.from_bytes(f.read(4), 'big')
        # rest is the label data, each label is stored as unsigned byte
        # label values are 0 to 9
        label_data = f.read()
        labels = np.frombuffer(label_data, dtype=np.uint8)
        return labels


# Get dataset from the directory
images = get_images("train-images-idx3-ubyte.gz")
labels = get_labels("train-labels-idx1-ubyte.gz")

# init the model
model = tf.keras.models.Sequential([

    # Flatten the image to 784 input pixels
    tf.keras.layers.Flatten(input_shape=(28, 28, 1)),

    # Add three hidden layers
    tf.keras.layers.Dense(128, activation='leaky_relu'),  # 128 best
    tf.keras.layers.Dense(64, activation='leaky_relu'),
    tf.keras.layers.Dense(64, activation='leaky_relu'),
    # Output nodes
    tf.keras.layers.Dense(10)
])

# Compile using Adam optimizer with learning rate 0.001
model.compile(
    optimizer=tf.keras.optimizers.Adam(0.001),
    loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
    metrics=[tf.keras.metrics.SparseCategoricalAccuracy()],
)

# Fit the model to the data
history1 = model.fit(
    images,
    labels,
    epochs=40,
    validation_split=0.3,  # 30% validation data
    batch_size=32  # batch size of 32
)
