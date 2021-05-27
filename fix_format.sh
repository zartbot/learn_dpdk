find ./ -name "*" | xargs dos2unix
find ./ -name "*.c" | xargs sed -i 's/\t/    /g'
find ./ -name "*.h" | xargs sed -i 's/\t/    /g'
