# Leaf-Class: A Lightweight Google Classroom Wrapper

Leaf-Class is a lightweight Google Classroom wrapper built in C using GTK and WebKit2GTK. It provides a native application interface for accessing Google Classroom, offering features like theming, DarkReader integration, popup-based Google login, and a built-in Download Manager.

## Key Features & Benefits

- **Lightweight:** Built in C for optimal performance and minimal resource consumption.
- **Theming:** Supports customizable themes for personalized user experience (light and dark themes included).
- **DarkReader Integration:** Leverages DarkReader to provide a comfortable dark mode experience.
- **Popup-Based Google Login:** Simplifies the authentication process with popup-based Google login.
- **Built-in Download Manager:** Manages and tracks file downloads within the application.
- **Native Application Interface:** Provides a native application feel compared to using Google Classroom in a web browser.

## Prerequisites & Dependencies

Before installing Leaf-Class, ensure you have the following dependencies installed on your system:

- **GTK (>=3.0):**  The GTK library is required for the graphical user interface.
- **WebKit2GTK:** The WebKit2GTK library is required for rendering web content.
- **CMake:**  CMake is used for building the project.
- **A C compiler (e.g., GCC or Clang):**  A C compiler is necessary for compiling the source code.
- **Make:** Used to build the project after CMake configuration.

**Debian/Ubuntu Example Installation:**

```bash
sudo apt-get update
sudo apt-get install libgtk-3-dev libwebkit2gtk-4.0-dev cmake build-essential
```

**Fedora Example Installation:**
```bash
sudo dnf install gtk3-devel webkit2gtk3-devel cmake make gcc
```

## Installation & Setup Instructions

Follow these steps to install and set up Leaf-Class:

1.  **Clone the Repository:**

    ```bash
    git clone https://github.com/hasan-psl/Leaf-Class.git
    cd Leaf-Class
    ```

2.  **Create a Build Directory:**

    ```bash
    mkdir build
    cd build
    ```

3.  **Configure the Project with CMake:**

    ```bash
    cmake ..
    ```

4.  **Build the Project:**

    ```bash
    make
    ```

5.  **Install the Application (optional):**

    ```bash
    sudo make install
    ```

    This will install the `leaf-class` executable to `/usr/bin` and desktop entry to `/usr/share/applications` making it accessible from your desktop environment.

## Usage Examples

After installation, you can run Leaf-Class from your terminal:

```bash
leaf-class
```

Or, if you installed the application, you can find it in your application menu.

The application will open a window where you can log in to your Google Classroom account and access your classrooms.

## Configuration Options

Leaf-Class supports theming.  The application can switch between light and dark themes.

The CSS files for the themes are located in the `leaf-class/css/` directory:

-   `leaf-class/css/light.css`: Defines the styles for the light theme.
-   `leaf-class/css/dark.css`: Defines the styles for the dark theme.

The Javascript file used for DarkReader injection is located in the root.

- `darkreader.js`:  Dark Reader script.

## Project Structure

```
├── CMakeLists.txt
├── LICENSE
├── darkreader.js
└── debian/
└── leaf-class/
└── DEBIAN/
├── control
└── opt/
└── leaf-class/
├── leaf-class
└── usr/
└── bin/
├── leaf-class
└── share/
└── applications/
├── leaf-class.desktop
└── icons/
├── leaf-class.png
└── leaf-class/
└── css/
```

## Contributing Guidelines

Contributions are welcome!  If you would like to contribute to Leaf-Class, please follow these guidelines:

1.  **Fork the repository.**
2.  **Create a new branch for your feature or bug fix.**
3.  **Make your changes and commit them with clear and concise commit messages.**
4.  **Submit a pull request to the main branch.**

Please ensure that your code adheres to the coding style of the project and includes appropriate tests.

## License Information

Leaf-Class is licensed under the MIT License.  See the `LICENSE` file for more information.

```
MIT License

Copyright (c) 2024 hasan-psl

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Acknowledgments

This project utilizes:

-   **GTK:** For the graphical user interface.
-   **WebKit2GTK:** For rendering web content.
-   **DarkReader:** For providing the dark mode functionality.