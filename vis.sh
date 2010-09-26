#java -jar tools/PlayGame.jar maps/map${2}.txt 1000 200 log.txt ./MyBot "java -jar ${1}" > vis_output
java -jar tools/PlayGame.jar maps/map${2}.txt 1000 300 log.txt ./galcon "java -jar ${1}" > vis_output

cat vis_output | java -jar tools/ShowGame.jar

