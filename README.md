# CSC 415 File System

A complete Unix-like file system implementation in C, featuring hierarchical directory structures, persistent storage, bitmap-based free space management, and buffered file I/O operations.

## Overview

This project implements a fully functional file system from scratch, built on top of a simulated block storage device. The file system provides standard Unix-like operations including file and directory management, navigation, and persistent data storage across sessions. It demonstrates core operating systems concepts including:

- Volume management and formatting
- Free space allocation and deallocation
- Directory hierarchy and path resolution
- Buffered file I/O with seek operations
- Metadata tracking (timestamps, permissions, file sizes)
- Data persistence using LBA (Logical Block Addressing)

The file system operates on a virtual volume file, simulating a physical disk with configurable size and block size parameters.

## Architecture

### Core Components

#### 1. Volume Control Block (VCB)
Located at block 0, the VCB contains critical metadata about the entire file system:
- Volume name and signature for validation
- Block size (512 bytes) and total block count
- Starting locations for the free space bitmap and root directory
- Used to determine if a volume is already formatted or needs initialization

#### 2. Free Space Management
A bitmap-based allocation system occupying blocks 1-40:
- Each bit represents one block's availability (0 = free, 1 = used)
- Supports allocation of contiguous or scattered blocks
- Persistent across sessions - written to disk after every allocation/deallocation
- Reserves the first 41 blocks for system use (VCB + bitmap)

#### 3. Directory Structure
Hierarchical directory system with fixed-size directory entries:
- Each directory contains 32 entries (de_struct)
- Directory entries include `.` (self) and `..` (parent) for navigation
- Each entry stores: filename (256 chars), size, mode/permissions, block locations, timestamps
- Directories are stored as files containing arrays of directory entries
- Supports both absolute and relative path resolution

#### 4. File Operations (Buffered I/O)
Efficient file access through buffering:
- File Control Blocks (FCB) track open files with 512-byte buffers
- Supports standard operations: open, read, write, seek, close
- Open flags: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND
- Files can grow dynamically up to 182 blocks (~90 KB)

#### 5. Low-Level Storage Interface
Block-level I/O abstraction:
- LBAread/LBAwrite functions for reading/writing 512-byte blocks
- All file system data persists in a single volume file on the host OS
- Simulates physical disk operations

### Data Structures

**Directory Entry (de_struct)**
```c
typedef struct de_struct {
    char file_name[256];                    // Filename (max 255 characters)
    size_t size;                            // Size in bytes
    mode_t mode;                            // Permissions
    int blocks_allocated[182];              // Array of block numbers
    int blocks_count;                       // Number of blocks used
    time_t date_created;                    // Creation timestamp
    time_t date_modified;                   // Last modified timestamp
    int is_directory;                       // 1 = directory, 0 = file
} de_struct;
```

**Volume Control Block (vcb_struct)**
```c
typedef struct vcb_struct {
    char volume_name[64];                   // Volume identifier
    size_t block_size;                      // Bytes per block (512)
    int block_count;                        // Total blocks in volume
    int freespace_list_start;               // Bitmap starting block
    int root_dir_start;                     // Root directory location
    long long signature;                    // Validation signature
} vcb_struct;
```

## Key Features

### Persistence
- File system state survives across program restarts
- Volume signature verification on startup
- Automatic loading of existing volumes or formatting new ones

### Free Space Management
- Bitmap tracks all blocks in the volume
- First-fit allocation strategy
- Automatic rollback on allocation failures
- Blocks are zeroed when freed for security

### Directory Operations
- Create and remove directories (mkdir, rmdir)
- Navigate directories with absolute or relative paths
- Current working directory (cwd) tracking
- List directory contents with metadata

### File Operations
- Create, read, write, and delete files
- Random access with seek operations (SEEK_SET, SEEK_CUR, SEEK_END)
- Buffered I/O for efficient access
- Multiple open modes with flag support

### Path Resolution
- Supports both absolute (`/dir/file`) and relative (`../dir/file`) paths
- Handles `.` (current) and `..` (parent) directory entries
- Path parsing with parent directory tracking

## Supported Commands

The file system includes an interactive shell (`fsshell`) with the following commands:

| Command | Description |
|---------|-------------|
| `ls [path]` | List files and directories |
| `cp <source> <dest>` | Copy a file within the file system |
| `mv <source> <dest>` | Move/rename a file or directory |
| `md <dirname>` | Create a new directory (mkdir) |
| `rm <path>` | Remove a file or directory |
| `touch <filename>` | Create an empty file |
| `cat <filename>` | Display file contents |
| `cp2l <fs_path> <linux_path>` | Copy file from file system to Linux |
| `cp2fs <linux_path> <fs_path>` | Copy file from Linux to file system |
| `cd <path>` | Change current directory |
| `pwd` | Print current working directory |
| `history` | Display command history |
| `help` | Show available commands |
| `exit` / `quit` | Exit the shell |

## How to Run

### Prerequisites
- GCC compiler
- Linux/Unix environment (or WSL on Windows)
- readline library

### Building the Project

```bash
# Compile the file system
make

# Run with default parameters (10MB volume, 512-byte blocks)
make run

# Run with custom parameters
./fsshell <volume_file> <volume_size> <block_size>

# Example: Create a 5MB volume
./fsshell MyVolume 5000000 512

# Clean build artifacts
make clean
```

### Volume Parameters
- **volume_file**: Name of the file to use as the virtual disk
- **volume_size**: Total size in bytes (500,000 to 10,000,000 recommended)
- **block_size**: Size of each block in bytes (must be 512)

### First Run
On first execution with a new volume file, the system will:
1. Format the volume and initialize the VCB
2. Create the free space bitmap
3. Initialize the root directory
4. Set current working directory to root (`/`)

On subsequent runs, it will:
1. Detect the existing signature
2. Load the VCB and free space map
3. Load the root directory
4. Resume from the previous state

## Implementation Details

### File System Layout
```
Block 0:           Volume Control Block (VCB)
Blocks 1-40:       Free Space Bitmap
Blocks 41+:        User data (directories and files)
```

### Directory Entry Storage
- Each directory contains 32 entries
- Directories occupy 64 blocks (32 entries × 1024 bytes / 512 bytes per block)
- First two entries always reserved for `.` and `..`

### File Size Limits
- Maximum file size: ~90 KB (182 blocks × 512 bytes)
- Maximum filename length: 255 characters
- Maximum path length: 256 characters
- Maximum open files: 20 (configurable via MAXFCBS)

## Project Structure

```
.
├── fsshell.c           # Interactive shell and main driver
├── fsInit.c            # File system initialization and formatting
├── mfs.c/h             # Directory operations and file system interface
├── b_io.c/h            # Buffered file I/O operations
├── freeSpace.c/h       # Free space bitmap management
├── fsLow.h             # Low-level LBA read/write interface
├── fsLow.o             # Precompiled LBA implementation (x86_64)
├── fsLowM1.o           # Precompiled LBA implementation (ARM64)
├── Makefile            # Build configuration
└── Hexdump/            # Utility for analyzing volume files

```

## Technical Highlights

### Robust Error Handling
- Validates all disk I/O operations
- Checks for allocation failures and rolls back on errors
- Prevents corruption of system-reserved blocks

### Memory Management
- Careful allocation and deallocation of buffers
- Cleanup on exit to prevent memory leaks
- Buffer reuse for efficiency

### Consistency
- Free space bitmap synchronized with disk after every operation
- Directory metadata updated atomically
- File system state always consistent on disk

## Authors

**Team: Debug Thugs**
- Randy Chen (922525848)
- Michael Thompson (922707016)
- Eric Ahsue (922711514)
- Utku Tarhan (918371654)

**Course**: CSC-415 Operating Systems - Spring 2025
**Institution**: San Francisco State University
**Instructor**: Professor Bierman

## License

This project is licensed under the terms in the LICENSE file.

## Acknowledgments

- Low-level LBA implementation provided by Professor Bierman
- Project structure based on CSC 415 File System assignment specifications
