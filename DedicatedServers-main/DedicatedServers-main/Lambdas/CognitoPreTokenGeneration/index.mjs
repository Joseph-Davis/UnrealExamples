export const handler = async (event) => {
  console.log(JSON.stringify(event), null, 2);

  // Check if the event is for access token customization
  if (event.triggerSource === 'TokenGeneration_Authentication') {
    // add a custom scope to the access token
    event.response = {
      "claimsAndScopeOverrideDetails": {
        "idTokenGeneration": {},
        "accessTokenGeneration": {
          "claimsToAddOrOverride": {},
          "scopesToAdd": [
            "dedicated_servers_scope"
          ]
        }
      }
    };
  }
  console.log('After adding the custom scope:', JSON.stringify(event, null, 2));
  return event;
};
