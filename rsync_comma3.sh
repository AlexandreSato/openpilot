#!/bin/bash

BASE_DIR="/home/sato/.comma/media/0"
C3_IP="192.168.15.7"
LOG_FILE="/home/sato/.comma/comma_sync.log"
LOCK_FILE="/tmp/comma_sync.lock"
MAX_RETRIES=3
RETRY_DELAY=30

# Prevent simultaneous executions
if [ -f "$LOCK_FILE" ]; then
    echo "❌ Script already running. Exiting." | tee -a "$LOG_FILE"
    exit 1
fi
touch "$LOCK_FILE"

cleanup() {
    rm -f "$LOCK_FILE" /tmp/remote_files.txt /tmp/local_files.txt /tmp/missing_files.txt /tmp/rsync_files.txt
    echo "===== Cleanup done =====" | tee -a "$LOG_FILE"
}
trap cleanup EXIT

echo "===== $(date) =====" | tee -a "$LOG_FILE"

if ping -c1 -W5 $C3_IP > /dev/null 2>&1; then
    echo "✅ Comma3 reachable" | tee -a "$LOG_FILE"

    # Get file lists for comparison
    echo "🔍 Checking file synchronization..." | tee -a "$LOG_FILE"

    # Get remote files list from Comma3
    ssh -p 22 -o ConnectTimeout=30 comma@$C3_IP "find /data/media/0/realdata -type f ! -name '*.hevc' -printf '%P\n'" 2>/dev/null | sort > /tmp/remote_files.txt
    remote_count=$(wc -l < /tmp/remote_files.txt)

    # Get local files list
    find "$BASE_DIR/realdata" -type f ! -name '*.hevc' -printf '%P\n' 2>/dev/null | sort > /tmp/local_files.txt

    # Find missing files (on Comma3 but not on computer)
    comm -23 /tmp/remote_files.txt /tmp/local_files.txt > /tmp/missing_files.txt
    missing_count=$(wc -l < /tmp/missing_files.txt)

    echo "📊 Synchronization status:" | tee -a "$LOG_FILE"
    echo "   Files on Comma3: $remote_count" | tee -a "$LOG_FILE"
    echo "   Missing on computer: $missing_count" | tee -a "$LOG_FILE"

    # Sync missing files if any
    if [ "$missing_count" -gt 0 ]; then
        echo "🔄 Downloading $missing_count missing files..." | tee -a "$LOG_FILE"

        # Show first few missing files
        echo "📝 Missing files:" | tee -a "$LOG_FILE"
        head -5 /tmp/missing_files.txt | while read file; do
            echo "   - $file" | tee -a "$LOG_FILE"
        done

        for attempt in $(seq 1 $MAX_RETRIES); do
            echo "Attempt $attempt of $MAX_RETRIES..." | tee -a "$LOG_FILE"

            # Create rsync file list with proper paths
            sed 's/^/realdata\//' /tmp/missing_files.txt > /tmp/rsync_files.txt

            if /usr/bin/rsync -e 'ssh -p 22 -o ConnectTimeout=30' \
                --files-from=/tmp/rsync_files.txt \
                --progress \
                --human-readable \
                --timeout=300 \
                --partial \
                --append \
                --log-file="$LOG_FILE" \
                -av \
                comma@$C3_IP:/data/media/0/ \
                "$BASE_DIR/"; then

                echo "✅ Download completed" | tee -a "$LOG_FILE"

                # Verify download
                downloaded_count=0
                while read file; do
                    if [ -f "$BASE_DIR/realdata/$file" ]; then
                        ((downloaded_count++))
                    fi
                done < /tmp/missing_files.txt

                echo "📦 Downloaded $downloaded_count files" | tee -a "$LOG_FILE"
                break
            else
                RSYNC_EXIT=$?
                echo "⚠️ Download failed (attempt $attempt)" | tee -a "$LOG_FILE"

                if [ $attempt -eq $MAX_RETRIES ]; then
                    echo "❌ All download attempts failed" | tee -a "$LOG_FILE"
                    break
                else
                    sleep $RETRY_DELAY
                fi
            fi
        done
    else
        echo "✅ All Comma3 files are synchronized" | tee -a "$LOG_FILE"
    fi

    # Final check
    final_missing=$(comm -23 /tmp/remote_files.txt /tmp/local_files.txt | wc -l)
    if [ "$final_missing" -eq 0 ]; then
        echo "🎉 Synchronization complete: All files are up to date" | tee -a "$LOG_FILE"
    else
        echo "⚠️ Still missing $final_missing files" | tee -a "$LOG_FILE"
    fi

    # Cleanup old files
    echo "🗑️ Removing files older than 90 days..." | tee -a "$LOG_FILE"
    old_files=$(find "$BASE_DIR" -type f -mtime +90 | wc -l)
    if [ "$old_files" -gt 0 ]; then
        find "$BASE_DIR" -type f -mtime +90 -delete 2>/dev/null
        echo "✅ Removed $old_files old files" | tee -a "$LOG_FILE"
    fi

else
    echo "❌ Comma3 not reachable" | tee -a "$LOG_FILE"
    exit 1
fi

echo "===== Done =====" | tee -a "$LOG_FILE"