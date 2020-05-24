#!/usr/bin/bash
set -u
echo -n "Starting runme.sh at " && date
echo "--------------------------------------------------"


# Compile compute program & run it
if [ -f make.sh ]; then
    echo "Compiling..."
    chmod +x make.sh
    ./make.sh
    if [ -f compute ]; then
        echo -n "Executing compute at " && date

		./compute

        echo -n "Executing compute has finished at " && date
    else
        echo "compute not found!"
    fi
else
    echo "make.sh not found!"
fi


# Spawn a script to upload all final logs
declare -r RNG_UUID=$(head -c 500 /dev/urandom | tr -dc 'a-z0-9' | fold -w 8 | head -n 1)
declare -r CUR_DATE=$(date +"%m-%d-%Y_%H-%M-%S")
cat << EOF > final.sh
#!/usr/bin/bash
sleep 5s
aws s3 cp output.txt s3://your_bucket_name/logs/output_${CUR_DATE}_${RNG_UUID}.txt
aws s3 cp runme.txt s3://your_bucket_name/logs/runme_${CUR_DATE}_${RNG_UUID}.txt
sleep 5s
sudo poweroff
EOF
chmod +x final.sh
./final.sh &


echo -n "Stopping runme.sh at " && date
echo "--------------------------------------------------"