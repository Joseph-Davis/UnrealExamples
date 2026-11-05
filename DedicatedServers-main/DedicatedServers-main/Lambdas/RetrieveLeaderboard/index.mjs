import { DynamoDBClient, ScanCommand } from "@aws-sdk/client-dynamodb";
import { unmarshall } from "@aws-sdk/util-dynamodb"

export const handler = async (event) => {
  const dynamoDBClient = new DynamoDBClient( { region: process.env.REGION } );
  const scanCommand = new ScanCommand({
    TableName: "Leaderboard"
  });
  try {
    const scanResponse = await dynamoDBClient.send(scanCommand);
    const leaderboard = scanResponse.Items.map(item => unmarshall(item));
    return { Leaderboard: leaderboard }
  } catch(error) {
    return error;
  }
};
