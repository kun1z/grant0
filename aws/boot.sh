#!/usr/bin/bash
set -u
echo -n "Starting boot.sh at " && date
echo "--------------------------------------------------"

declare -i ERROR=0

echo "Installing updates"
yum update -y

echo "Installing GCC"
yum install -y gcc

echo "Installing htop"
yum install -y htop

lscpu
lsblk

echo "Checking if GCC is installed"
if type "gcc" &> /dev/null; then
    echo "GCC is installed"
    gcc --version
    if [ -d "/home/ec2-user" ]; then
        echo "Downloading /boot folder"
        aws s3 sync s3://your_bucket_name/boot /home/ec2-user/boot
        if [ $? -eq 0 ]; then
            if [ -d "/home/ec2-user/boot" ]; then
                echo "/boot folder has been downloaded"
                if [ -f "/home/ec2-user/boot/runme.sh" ]; then
                    echo -n "Executing runme.sh as ec2-user..."
                    chown -R ec2-user:ec2-user /home/ec2-user/boot
                    chmod +x /home/ec2-user/boot/runme.sh
                    su -c "cd /home/ec2-user/boot && ./runme.sh &> runme.txt" -l ec2-user &
                    echo "Done!"
                else
                    echo "FATAL: No script to run in the boot folder"
                    ERROR=1
                fi
            else
                echo "FATAL: Boot folder does not exist"
                ERROR=1
            fi
        else
            echo "FATAL: S3 sync failed"
            ERROR=1
        fi
    else
        echo "FATAL: User folder does not exist"
        ERROR=1
    fi
else
    echo "FATAL: GCC is not installed"
    ERROR=1
fi

declare -r RNG_UUID=$(head -c 500 /dev/urandom | tr -dc 'a-z0-9' | fold -w 8 | head -n 1)
declare -r CUR_DATE=$(date +"%m-%d-%Y_%H-%M-%S")

if [ $ERROR -eq 1 ]; then
    echo -n "Creating error.sh to upload final logs..."
    cat << EOF > /home/error.sh
#!/usr/bin/bash
sleep 5s
aws s3 cp /var/log/cloud-init-output.log s3://your_bucket_name/logs/error_${CUR_DATE}_${RNG_UUID}.txt
sleep 5s
poweroff
EOF
    chmod +x /home/error.sh
    /home/error.sh &
    echo "Done!"
else
    echo -n "Creating bootlog.sh to upload boot logs..."
    cat << EOF > /home/bootlog.sh
#!/usr/bin/bash
sleep 5s
aws s3 cp /var/log/cloud-init-output.log s3://your_bucket_name/logs/boot_${CUR_DATE}_${RNG_UUID}.txt
EOF
    chmod +x /home/bootlog.sh
    /home/bootlog.sh &
    echo "Done!"
fi

echo -n "Stopping boot.sh at " && date
echo "--------------------------------------------------"