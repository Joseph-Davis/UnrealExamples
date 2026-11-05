import { GameLiftClient, ListFleetsCommand, DescribeFleetAttributesCommand, DescribeGameSessionsCommand, CreateGameSessionCommand } from "@aws-sdk/client-gamelift";

export const handler = async (event) => {
  const gameLiftClient = new GameLiftClient( { region: process.env.REGION } );

  try {
    const listFleetsInput = {
      Limit: 10
    };
    const listFleetsCommand = new ListFleetsCommand(listFleetsInput);
    const listFleetsResponse = await gameLiftClient.send(listFleetsCommand);
    const fleetIds = listFleetsResponse.FleetIds;

    const describeFleetAttributesInput = { // DescribeFleetAttributesInput
      FleetIds: fleetIds,
      Limit: 10
    };

    const describeFleetAttributesCommand = new DescribeFleetAttributesCommand(describeFleetAttributesInput);
    const describeFleetAttributesResponse = await gameLiftClient.send(describeFleetAttributesCommand);
    const fleetAttributes = describeFleetAttributesResponse.FleetAttributes;

    let fleetId;
    for (const fleetAttribute of fleetAttributes) {
      if (fleetAttribute.Status === "ACTIVE") {
        fleetId = fleetAttribute.FleetId;
        break;
      }
    }

    const describeGameSessionsInput = {
      FleetId: fleetId,
      Limit: 10,
      Location: "custom-home-desktop", // remove for EC2
      StatusFilter: "ACTIVE",
    };

    const describeGameSessionsCommand = new DescribeGameSessionsCommand(describeGameSessionsInput);
    const describeGameSessionsResponse = await gameLiftClient.send(describeGameSessionsCommand);
    const gameSessions = describeGameSessionsResponse.GameSessions;

    let gameSession;
    for (const session of gameSessions) {
      if (session.CurrentPlayerSessionCount < session.MaximumPlayerSessionCount && session.PlayerSessionCreationPolicy === "ACCEPT_ALL") {
        gameSession = session;
        break;
      }
    }

    if (gameSession) {
      // found an active game session with room for more players
    } else {
      // no game session found, create one.
      const createGameSessionInput = {
        GameProperties: [
          {
            Key: "difficulty", 
            Value: "novice", 
          },
        ],
        FleetId: fleetId,
        MaximumPlayerSessionCount: 20,
        Location: "custom-home-desktop"
      };
      const createGameSessionCommand = new CreateGameSessionCommand(createGameSessionInput);
      const createGameSessionResponse = await gameLiftClient.send(createGameSessionCommand);
      gameSession = createGameSessionResponse.GameSession;
    }

    return gameSession;
  } catch(error) {
    return error;
  }

  


};
