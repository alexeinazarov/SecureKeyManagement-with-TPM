import re
import sys

def post_process_supervisor_conf(conf_file):
    print("Post-processing supervisord.conf to correct malformed paths with Python...")

    with open(conf_file, 'r') as file:
        config = file.read()

    # Extract the command line and its directory path
    command_match = re.search(r'command=(.+)', config)
    if command_match:
        command_path = command_match.group(1)
        command_dir = command_path.rsplit('/', 1)[0]  # Get the directory without the filename

        # Update the directory path in the configuration
        config = re.sub(r'directory=.*/build', f'directory={command_dir}', config)

        # Verification to ensure the correction
        if re.search(f'directory={command_dir}', config):
            print("Verification successful: Corrected path found in configuration.")
        else:
            print("Verification failed: Corrected path not found in configuration.")
    else:
        print("Error: 'command' not found in configuration.")
        sys.exit(1)

    # Find all command lines and add sudo to [program:kms_server] section
    config = re.sub(r'(\[program:kms_server\]\s+command=)(.+)', r'\1sudo \2', config)

    with open(conf_file, 'w') as file:
        file.write(config)

    print("Post-processing completed.")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: post_process_supervisor_conf.py <path_to_supervisord.conf>")
        sys.exit(1)
    
    conf_file = sys.argv[1]
    post_process_supervisor_conf(conf_file)

