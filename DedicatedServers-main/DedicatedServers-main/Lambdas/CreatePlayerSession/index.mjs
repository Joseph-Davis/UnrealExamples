import { GameLiftClient, CreatePlayerSessionCommand } from "@aws-sdk/client-gamelift";

export const handler = async (event) => {
  try {
    const gameLiftClient = new GameLiftClient( { region: process.env.REGION } );
    const createPlayerSessionInput = {
      GameSessionId: event.gameSessionId,
      PlayerId: event.playerId,
      Location: "custom-home-desktop" // Remove this for EC2 fleets
    };
    const createPlayerSessionCommand = new CreatePlayerSessionCommand(createPlayerSessionInput);
    const createPlayerSessionResponse = await gameLiftClient.send(createPlayerSessionCommand);
    return createPlayerSessionResponse;
  } catch(error) {
    return error;
  }
};
