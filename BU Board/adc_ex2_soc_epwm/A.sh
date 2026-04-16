#!/bin/bash
 
# --- CONFIGURATION ---

# URL Encoding for Special Characters (@ becomes %40)

USER_ENC="parthasarathy.m%40deltaww.com"

PASS_ENC="Delta%40123"

REPO_DOMAIN="gitlab.deltaww.com/ictbg/cisbu/dinrdmcis/ups/pdu-gen-3/powercalculation.git"

DIR_NAME="powercalculation"
 
# Combine into the final URL (https://user:pass@domain/path)

FULL_URL="https://${USER_ENC}:${PASS_ENC}@${REPO_DOMAIN}"
 
# 1. Clone the repository

echo "Cloning repository..."

# -q suppresses the URL output to hide credentials from the screen

git clone -q "$FULL_URL"
 
# Check if clone succeeded

if [ ! -d "$DIR_NAME" ]; then

    echo "Error: Directory '$DIR_NAME' not found. Cloning likely failed (Check VPN/Internet/Credentials)."

    exit 1

fi
 
# 2. Go into the directory

cd "$DIR_NAME" || exit
 
# 3. Handle Branch Selection

if [ -n "$1" ]; then

    # Use argument if provided

    BRANCH_NAME="$1"

    echo "Branch selected: $BRANCH_NAME"

else

    # List available remote branches

    echo "----------------------------------------"

    echo "      AVAILABLE REMOTE BRANCHES         "

    echo "----------------------------------------"

    # List remote branches, remove 'origin/', and clean up formatting

    git branch -r | grep -v '\->' | sed 's/origin\///' | sed 's/^[[:space:]]*//'

    echo "----------------------------------------"

    read -p "Enter the branch name to switch to: " BRANCH_NAME

fi
 
# Validate input

if [ -z "$BRANCH_NAME" ]; then

    echo "Error: No branch name provided. Exiting."

    exit 1

fi
 
# 4. Switch to the branch

echo "Switching to branch '$BRANCH_NAME'..."

if ! git checkout "$BRANCH_NAME"; then

    echo "Error: Failed to switch to '$BRANCH_NAME'. Please ensure the name matches the list above."

    exit 1

fi
 
# 5. Move ALL files (Visible AND Hidden)

echo "Moving all files to parent directory..."
 
# --- CRITICAL STEP FOR HIDDEN FILES ---

# 'dotglob' allows '*' to match hidden files (like .git, .gitignore)

shopt -s dotglob
 
# Move everything (including hidden) to the parent directory

mv * ../
 
# Disable dotglob to return to standard Bash behavior

shopt -u dotglob

# --------------------------------------
 
# 6. Delete the empty folder

cd ..

echo "Removing empty directory '$DIR_NAME'..."
 
# rmdir is safe: it will ONLY delete the folder if it is truly empty.

# If hidden files failed to move, this command will fail and alert you.

if rmdir "$DIR_NAME"; then

    echo "Success: '$DIR_NAME' removed."

else

    echo "Warning: Could not remove '$DIR_NAME'. It might not be empty."

fi
 
echo "Operation complete."


README.md (Visible files)
 
.git/ (The hidden Git folder — essential for git history)
 
.gitignore (Hidden config files)
 
Safety Check: I used rmdir at the end. If for some reason a hidden file did not move, rmdir will fail and say "Directory not empty," preventing you from losing data.
 