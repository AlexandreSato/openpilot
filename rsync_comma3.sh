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
    rm -f "$LOCK_FILE" /tmp/remote_files.txt /tmp/local_files.txt /tmp/missing_files.txt
    echo "===== Cleanup done =====" | tee -a "$LOG_FILE"
}
trap cleanup EXIT

echo "===== $(date) =====" | tee -a "$LOG_FILE"

# Check disk space
MIN_SPACE_GB=10
available_space_gb=$(df -h "$BASE_DIR" | awk 'NR==2 {print $4}' | sed 's/G//')
if (( $(echo "$available_space_gb < $MIN_SPACE_GB" | bc -l) )); then
    echo "❌ Not enough disk space (${available_space_gb}GB available, need ${MIN_SPACE_GB}GB)" | tee -a "$LOG_FILE"
    exit 1
fi
echo "✅ Disk space available: ${available_space_gb}GB" | tee -a "$LOG_FILE"

if ping -c1 -W5 $C3_IP > /dev/null 2>&1; then
    echo "✅ Ping successful" | tee -a "$LOG_FILE"

    # Get accurate counts
    echo "🔍 Getting file counts..." | tee -a "$LOG_FILE"

    remote_count=$(ssh -p 22 -o ConnectTimeout=30 comma@$C3_IP "find /data/media/0/realdata -type f ! -name '*.hevc' | wc -l" 2>/dev/null || echo "0")
    local_count=$(find "$BASE_DIR/realdata" -type f ! -name '*.hevc' | wc -l 2>/dev/null || echo "0")

    echo "Remote: $remote_count files, Local: $local_count files" | tee -a "$LOG_FILE"

    if [ "$local_count" -ge "$remote_count" ]; then
        echo "✅ No missing files" | tee -a "$LOG_FILE"
    else
        missing_count=$((remote_count - local_count))
        echo "📋 Missing files: $missing_count" | tee -a "$LOG_FILE"

        # Create file lists with RELATIVE PATHS from realdata/
        echo "🔍 Creating file lists with relative paths..." | tee -a "$LOG_FILE"

        # Remote files: get paths relative to /data/media/0/realdata/
        ssh -p 22 -o ConnectTimeout=30 comma@$C3_IP "find /data/media/0/realdata -type f ! -name '*.hevc' -printf 'realdata/%P\n'" 2>/dev/null | sort > /tmp/remote_files.txt

        # Local files: get paths relative to BASE_DIR
        find "$BASE_DIR/realdata" -type f ! -name '*.hevc' -printf '%P\n' 2>/dev/null | while read file; do
            echo "realdata/$file"  # Make paths match remote format
        done | sort > /tmp/local_files.txt

        # Find what's actually missing
        comm -23 /tmp/remote_files.txt /tmp/local_files.txt > /tmp/missing_files.txt
        actual_missing=$(wc -l < /tmp/missing_files.txt)

        echo "📋 Actually missing: $actual_missing files" | tee -a "$LOG_FILE"

        if [ "$actual_missing" -eq 0 ]; then
            echo "✅ No files actually missing" | tee -a "$LOG_FILE"
        else
            # Show first few missing files
            echo "📝 Sample missing files:" | tee -a "$LOG_FILE"
            head -3 /tmp/missing_files.txt | while read file; do
                echo "  - $file" | tee -a "$LOG_FILE"
            done

            # TRANSFER THE MISSING FILES
            echo "🔄 Transferring $actual_missing missing files..." | tee -a "$LOG_FILE"

            for attempt in $(seq 1 $MAX_RETRIES); do
                echo "Attempt $attempt of $MAX_RETRIES..." | tee -a "$LOG_FILE"

                # Use rsync with correct relative paths
                if /usr/bin/rsync -e 'ssh -p 22 -o ConnectTimeout=30' \
                    --files-from=/tmp/missing_files.txt \
                    --progress \
                    --human-readable \
                    --timeout=300 \
                    --partial \
                    --append \
                    --itemize-changes \
                    --log-file="$LOG_FILE" \
                    -avv \
                    comma@$C3_IP:/data/media/0/ \
                    "$BASE_DIR/"; then

                    echo "✅ Transfer completed on attempt $attempt" | tee -a "$LOG_FILE"
                    break
                else
                    RSYNC_EXIT=$?
                    echo "⚠️ Rsync failed with code $RSYNC_EXIT on attempt $attempt" | tee -a "$LOG_FILE"

                    if [ $attempt -eq $MAX_RETRIES ]; then
                        echo "❌ All transfer attempts failed" | tee -a "$LOG_FILE"
                    else
                        echo "Retrying in $RETRY_DELAY seconds..." | tee -a "$LOG_FILE"
                        sleep $RETRY_DELAY
                    fi
                fi
            done
        fi
    fi

    # Final verification
    echo "🔍 Final verification..." | tee -a "$LOG_FILE"
    final_count=$(find "$BASE_DIR/realdata" -type f ! -name '*.hevc' | wc -l 2>/dev/null || echo "0")
    final_size=$(du -sh "$BASE_DIR/realdata" 2>/dev/null | cut -f1 || echo "0B")

    echo "Final: $final_count files ($final_size)" | tee -a "$LOG_FILE"
    echo "Expected: $remote_count files" | tee -a "$LOG_FILE"

    if [ "$final_count" -eq "$remote_count" ]; then
        echo "🎉 Sync completed successfully!" | tee -a "$LOG_FILE"
    else
        echo "⚠️ Still missing $((remote_count - final_count)) files" | tee -a "$LOG_FILE"
    fi

    # Cleanup old files
    echo "🗑️ Cleaning up old files..." | tee -a "$LOG_FILE"
    find "$BASE_DIR" -type f -mtime +90 -delete 2>/dev/null

else
    echo "❌ Ping failed, Comma3 not reachable. Exiting." | tee -a "$LOG_FILE"
    exit 1
fi

echo "===== Done =====" | tee -a "$LOG_FILE"