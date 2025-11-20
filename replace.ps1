$content = Get-Content '\''src\main.cpp'\'' -Raw

$old = @'\''  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset='\''utf-8'\''>
  <meta name='\''viewport'\'' content='\''width=device-width, initial-scale=1'\''>
  <title>LN Markets Touch Config</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: '\''Segoe UI'\'', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #0a0a0a 0%, #1a1a1a 100%);
      color: #00ffff;
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 450px;
      margin: 0 auto;
      background: rgba(26, 26, 26, 0.95);
      padding: 30px;
      border-radius: 15px;
      border: 2px solid #ff0040;
      box-shadow: 0 8px 32px rgba(255, 0, 64, 0.3);
    }
    h1 {
      color: #ff0040;
      text-align: center;
      margin-bottom: 10px;
      font-size: 28px;
      font-weight: 300;
      text-shadow: 0 0 20px rgba(255, 0, 64, 0.5);
    }
    .subtitle {
      text-align: center;
      color: #00ffff;
      margin-bottom: 30px;
      font-size: 14px;
      opacity: 0.8;
    }

    /* Navigation par étapes */
    .steps {
      display: flex;
      justify-content: center;
      margin-bottom: 30px;
      position: relative;
    }
    .steps::before {
      content: '\'''\''';
      position: absolute;
      top: 15px;
      left: 20px;
      right: 20px;
      height: 2px;
      background: #333;
      z-index: 1;
    }
    .step {
      background: #333;
      color: #666;
      width: 30px;
      height: 30px;
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      font-weight: bold;
      position: relative;
      z-index: 2;
      transition: all 0.3s ease;
    }
    .step.active {
      background: #ff0040;
      color: white;
      box-shadow: 0 0 20px rgba(255, 0, 64, 0.5);
    }
    .step.completed {
      background: #00ff00;
      color: #000;
    }

    /* Sections */
    .section {
      display: none;
    }
    .section.active {
      display: block;
      animation: fadeIn 0.5s ease;
    }
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(20px); }
      to { opacity: 1; transform: translateY(0); }
    }

    /* Groupes de champs */
    .field-group {
      margin-bottom: 20px;
      position: relative;
    }
    .field-group.error input {
      border-color: #ff4444;
      box-shadow: 0 0 10px rgba(255, 68, 68, 0.3);
    }
    .field-group.success input {
      border-color: #00ff00;
      box-shadow: 0 0 10px rgba(0, 255, 0, 0.3);
    }

    label {
      display: block;
      margin-bottom: 8px;
      color: #00ffff;
      font-weight: 500;
      font-size: 14px;
    }

    input {
      width: 100%;
      padding: 12px 15px;
      background: rgba(10, 10, 10, 0.8);
      border: 2px solid #333;
      color: #fff;
      font-size: 16px;
      border-radius: 8px;
      transition: all 0.3s ease;
      font-family: '\''Courier New'\'', monospace;
    }
    input:focus {
      outline: none;
      border-color: #ff0040;
      box-shadow: 0 0 20px rgba(255, 0, 64, 0.3);
      background: rgba(20, 20, 20, 0.9);
    }
    input::placeholder {
      color: #666;
      font-style: italic;
    }

    /* Indicateurs de validation */
    .field-status {
      position: absolute;
      right: 15px;
      top: 40px;
      font-size: 18px;
      transition: all 0.3s ease;
    }
    .field-status.checking { color: #ffff00; }
    .field-status.valid { color: #00ff00; }
    .field-status.invalid { color: #ff4444; }

    /* Boutons spéciaux */
    .input-group {
      display: flex;
      gap: 10px;
    }
    .input-group input {
      flex: 1;
    }
    .btn-toggle {
      background: #333;
      border: 2px solid #555;
      color: #ccc;
      padding: 12px 15px;
      border-radius: 8px;
      cursor: pointer;
      transition: all 0.3s ease;
      font-size: 14px;
    }
    .btn-toggle:hover {
      background: #444;
      border-color: #666;
    }
    .btn-toggle.active {
      background: #ff0040;
      border-color: #ff0040;
      color: white;
    }

    /* Boutons de navigation */
    .nav-buttons {
      display: flex;
      justify-content: space-between;
      margin-top: 30px;
      gap: 15px;
    }
    .btn {
      padding: 12px 24px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s ease;
      flex: 1;
    }
    .btn:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .btn.secondary {
      background: #333;
      color: #ccc;
      border: 2px solid #555;
    }
    .btn.secondary:hover:not(:disabled) {
      background: #444;
      border-color: #666;
    }
    .btn.primary {
      background: #ff0040;
      color: white;
      border: 2px solid #ff0040;
    }
    .btn.primary:hover:not(:disabled) {
      background: #cc0033;
      border-color: #cc0033;
      box-shadow: 0 0 20px rgba(255, 0, 64, 0.5);
    }

    /* Bouton de test API */
    .test-api {
      margin-top: 20px;
      padding: 10px;
      background: rgba(0, 255, 0, 0.1);
      border: 1px solid #00ff00;
      border-radius: 8px;
      text-align: center;
    }
    .test-api.success {
      background: rgba(0, 255, 0, 0.2);
      border-color: #00ff00;
    }
    .test-api.error {
      background: rgba(255, 68, 68, 0.1);
      border-color: #ff4444;
    }

    /* Info box */
    .info {
      margin-top: 20px;
      padding: 15px;
      background: rgba(0, 255, 255, 0.1);
      border-left: 3px solid #00ffff;
      border-radius: 8px;
      font-size: 12px;
      line-height: 1.5;
    }
    .info h4 {
      color: #00ffff;
      margin-bottom: 8px;
      font-size: 14px;
    }

    /* Responsive */
    @media (max-width: 480px) {
      .container {
        margin: 10px;
        padding: 20px;
      }
      .nav-buttons {
        flex-direction: column;
      }
      .btn {
        margin-bottom: 10px;
      }
    }

    /* Animations */
    .shake {
      animation: shake 0.5s ease;
    }
    @keyframes shake {
      0%, 100% { transform: translateX(0); }
      25% { transform: translateX(-5px); }
      75% { transform: translateX(5px); }
    }
  </style>
</head>
<body>
  <div class='\''container'\''
    <h1>⚡ LN MARKETS TOUCH</h1>
    <div class='\''subtitle'\''>Configuration Terminal Trading</div>

    <!-- Indicateur d'\''étapes -->
    <div class='\''steps'\''
      <div class='\''step active'\'' id='\''step1'\''>1</div>
      <div class='\''step'\'' id='\''step2'\''>2</div>
      <div class='\''step'\'' id='\''step3'\''>3</div>
    </div>

    <form id='\''configForm'\''
      <!-- Étape 1: WiFi -->
      <div class='\''section active'\'' id='\''section1'\''
        <h3 style='\''color: #00ffff; text-align: center; margin-bottom: 20px;'\''>🌐 Configuration WiFi</h3>

        <div class='\''field-group'\'' id='\''ssidGroup'\''
          <label>Nom du réseau WiFi (SSID)</label>
          <input type='\''text'\'' id='\''ssid'\'' name='\''ssid'\'' placeholder='\''MonWiFi'\'' required>
          <div class='\''field-status'\'' id='\''ssidStatus'\'</div>
        </div>

        <div class='\''field-group'\'' id='\''passwordGroup'\''
          <label>Mot de passe WiFi</label>
          <div class='\''input-group'\''
            <input type='\''password'\'' id='\''password'\'' name='\''password'\'' placeholder='\''Mot de passe'\'' required>
            <button type='\''button'\'' class='\''btn-toggle'\'' id='\''toggleWifiPass'\'' title='\''Afficher/Masquer'\''>👁️</button>
          </div>
          <div class='\''field-status'\'' id='\''passwordStatus'\'</div>
        </div>
      </div>

      <!-- Étape 2: API Keys -->
      <div class='\''section'\'' id='\''section2'\''
        <h3 style='\''color: #00ffff; text-align: center; margin-bottom: 20px;'\''>🔑 Clés API LN Markets</h3>

        <div class='\''field-group'\'' id='\''apiKeyGroup'\''
          <label>API Key <span style='\''color: #888; font-size: 12px;'\''>(base64, ~44 caractères)</span></label>
          <input type='\''text'\'' id='\''api_key'\'' name='\''api_key'\'' placeholder='\''Votre clé API LN Markets'\'' spellcheck='\''false'\''
          <div class='\''field-status'\'' id='\''apiKeyStatus'\'</div>
        </div>

        <div class='\''field-group'\'' id='\''apiSecretGroup'\''
          <label>API Secret</label>
          <div class='\''input-group'\''
            <input type='\''password'\'' id='\''api_secret'\'' name='\''api_secret'\'' placeholder='\''Secret API (gardez-le secret!)'\'' spellcheck='\''false'\''
            <button type='\''button'\'' class='\''btn-toggle'\'' id='\''toggleSecret'\'' title='\''Afficher/Masquer'\''>👁️</button>
          </div>
          <div class='\''field-status'\'' id='\''apiSecretStatus'\'</div>
        </div>

        <div class='\''field-group'\'' id='\''apiPassphraseGroup'\''
          <label>Passphrase API</label>
          <div class='\''input-group'\''
            <input type='\''password'\'' id='\''api_passphrase'\'' name='\''api_passphrase'\'' placeholder='\''Votre passphrase'\'' spellcheck='\''false'\''
            <button type='\''button'\'' class='\''btn-toggle'\'' id='\''togglePassphrase'\'' title='\''Afficher/Masquer'\''>👁️</button>
          </div>
          <div class='\''field-status'\'' id='\''apiPassphraseStatus'\'</div>
        </div>

        <div class='\''test-api'\'' id='\''apiTest'\'' style='\''display: none;'\''>
          <div id='\''testResult'\''>🔍 Test de connexion API...</div>
        </div>
      </div>

      <!-- Étape 3: Lightning -->
      <div class='\''section'\'' id='\''section3'\''
        <h3 style='\''color: #00ffff; text-align: center; margin-bottom: 20px;'\''>⚡ Adresses Lightning</h3>

        <div class='\''field-group'\'' id='\''lightningGroup'\''
          <label>Adresse Lightning (retraits)</label>
          <input type='\''text'\'' id='\''lightning_address'\'' name='\''lightning_address'\'' placeholder='\''votre@wallet.com'\'' spellcheck='\''false'\''
          <div class='\''field-status'\'' id='\''lightningStatus'\'</div>
        </div>

        <div class='\''field-group'\'' id='\''depositGroup'\''
          <label>Adresse Lightning (dépôts)</label>
          <input type='\''text'\'' id='\''deposit_lnaddress'\'' name='\''deposit_lnaddress'\'' placeholder='\''votre@wallet.com'\'' spellcheck='\''false'\''
          <div class='\''field-status'\'' id='\''depositStatus'\'</div>
        </div>
      </div>

      <!-- Boutons de navigation -->
      <div class='\''nav-buttons'\''
        <button type='\''button'\'' class='\''btn secondary'\'' id='\''prevBtn'\'' disabled>⬅️ Précédent</button>
        <button type='\''button'\'' class='\''btn primary'\'' id='\''nextBtn'\''>Suivant ➡️</button>
      </div>

      <button type='\''submit'\'' class='\''btn primary'\'' id='\''saveBtn'\'' style='\''display: none; width: 100%; margin-top: 20px;'\''>💾 SAUVEGARDER CONFIGURATION</button>
    </form>

    <div class='\''info'\''
      <h4>📋 Informations</h4>
      <div id='\''infoContent'\''
        <p><strong>Étape 1:</strong> Configurez votre connexion WiFi pour accéder à Internet.</p>
        <p><strong>API Keys:</strong> Disponibles sur <a href='\''https://lnmarkets.com'\'' target='\''_blank'\'' style='\''color: #00ffff;'\''>lnmarkets.com</a> → Settings → API</p>
        <p><strong>Lightning:</strong> Format user@domain.com (ex: satoshi@lnmarkets.com)</p>
      </div>
    </div>
  </div>

  <script>
    // État de l'\''application
    let currentStep = 1;
    const totalSteps = 3;
    let formData = {};

    // Éléments DOM
    const steps = document.querySelectorAll('\''.step'\'');
    const sections = document.querySelectorAll('\''.section'\'');
    const prevBtn = document.getElementById('\''prevBtn'\'');
    const nextBtn = document.getElementById('\''nextBtn'\'');
    const saveBtn = document.getElementById('\''saveBtn'\'');
    const infoContent = document.getElementById('\''infoContent'\'');
    
    // Informations par étape
    const stepInfo = {
      1: "<p><strong>Étape 1:</strong> Configurez votre connexion WiFi pour accéder à Internet.</p><p>Le terminal a besoin d'\''Internet pour récupérer les prix Bitcoin et exécuter les trades.</p>",
      2: "<p><strong>Étape 2:</strong> Configurez vos clés API LN Markets.</p><p>Ces clés permettent au terminal de trader automatiquement. Gardez-les secrètes!</p><p><strong>Format attendu:</strong> API Key et Secret en base64 (~44 caractères)</p>",
      3: "<p><strong>Étape 3:</strong> Configurez vos adresses Lightning.</p><p>Ces adresses permettent les dépôts et retraits vers votre wallet Lightning personnel.</p><p><strong>Format:</strong> utilisateur@domaine.com</p>"
    };

    // Navigation entre étapes
    function showStep(step) {
      // Masquer toutes les sections
      sections.forEach(section => section.classList.remove('\''active'\''));
      steps.forEach(stepEl => stepEl.classList.remove('\''active'\'', '\''completed'\''));
      
      // Afficher l'\''étape courante
      document.getElementById(`section${step}`).classList.add('\''active'\'');
      document.getElementById(`step${step}`).classList.add('\''active'\'');
      
      // Marquer les étapes précédentes comme complétées
      for (let i = 1; i < step; i++) {
        document.getElementById(`step${i}`).classList.add('\''completed'\'');
      }
      
      // Mettre à jour les boutons
      prevBtn.disabled = step === 1;
      nextBtn.textContent = step === totalSteps ? '\''Terminer ✅'\'' : '\''Suivant ➡️'\'';
      saveBtn.style.display = step === totalSteps ? '\''block'\'' : '\''none'\'';
      
      console.log(`🔄 [JS] Étape ${step}/${totalSteps} affichée - Bouton sauvegarde: ${step === totalSteps ? '\''visible'\'' : '\''caché'\''}`);
      
      // Mettre à jour les infos
      infoContent.innerHTML = stepInfo[step];
      
      currentStep = step;
    }

    // Validation des champs
    function validateField(fieldId, validator) {
      const field = document.getElementById(fieldId);
      const group = field.closest('\''.field-group'\'');
      const status = document.getElementById(fieldId + '\''Status'\'');
      
      if (!field.value.trim()) {
        if (field.required) {
          group.className = '\''field-group error'\'';
          status.innerHTML = '\''❌'\'';
          status.className = '\''field-status invalid'\'';
          return false;
        } else {
          group.className = '\''field-group'\'';
          status.innerHTML = '\'''\''';
          return true;
        }
      }
      
      const isValid = validator ? validator(field.value) : true;
      
      if (isValid) {
        group.className = '\''field-group success'\'';
        status.innerHTML = '\''✅'\'';
        status.className = '\''field-status valid'\'';
        return true;
      } else {
        group.className = '\''field-group error'\'';
        status.innerHTML = '\''❌'\'';
        status.className = '\''field-status invalid'\'';
        return false;
      }
    }

    // Validateurs
    const validators = {
      ssid: (value) => value.length >= 1 && value.length <= 32,
      password: (value) => value.length >= 8,
      api_key: (value) => {
        if (!value) return true; // Optionnel
        return /^[A-Za-z0-9+/=]{20,}$/.test(value) && value.length >= 20;
      },
      api_secret: (value) => {
        if (!value) return true; // Optionnel
        return /^[A-Za-z0-9+/=]{20,}$/.test(value) && value.length >= 20;
      },
      api_passphrase: (value) => {
        if (!value) return true; // Optionnel
        return value.length >= 8 ; /^[A-Za-z0-9]+$/.test(value);
      },
      lightning_address: (value) => {
        if (!value) return true; // Optionnel
        const atIndex = value.indexOf('\''@'\'');
        return atIndex > 0 ; atIndex < value.length - 1 ; value.includes('\''.''\'');
      },
      deposit_lnaddress: (value) => {
        if (!value) return true; // Optionnel
        const atIndex = value.indexOf('\''@'\'');
        return atIndex > 0 ; atIndex < value.length - 1 ; value.includes('\''.''\'');
      }
    };

    // Validation en temps réel
    document.querySelectorAll('\''input'\'').forEach(input => {
      input.addEventListener('\''input'\'', () => {
        const validator = validators[input.id];
        validateField(input.id, validator);
      });
      
      input.addEventListener('\''blur'\'', () => {
        const validator = validators[input.id];
        validateField(input.id, validator);
      });
    });

    // Boutons toggle mot de passe
    document.getElementById('\''toggleWifiPass'\'').addEventListener('\''click'\'', function() {
      const input = document.getElementById('\''password'\'');
      input.type = input.type === '\''password'\'' ? '\''text'\'' : '\''password'\'';
      this.classList.toggle('\''active'\'');
    });

    document.getElementById('\''toggleSecret'\'').addEventListener('\''click'\'', function() {
      const input = document.getElementById('\''api_secret'\'');
      input.type = input.type === '\''password'\'' ? '\''text'\'' : '\''password'\'';
      this.classList.toggle('\''active'\'');
    });

    document.getElementById('\''togglePassphrase'\'').addEventListener('\''click'\'', function() {
      const input = document.getElementById('\''api_passphrase'\'');
      input.type = input.type === '\''password'\'' ? '\''text'\'' : '\''password'\'';
      this.classList.toggle('\''active'\'');
    });

    // Navigation
    prevBtn.addEventListener('\''click'\'', () => {
      if (currentStep > 1) {
        showStep(currentStep - 1);
      }
    });

    nextBtn.addEventListener('\''click'\'', () => {
      // Validation de l'\''étape courante
      const currentSection = document.getElementById(`section${currentStep}`);
      const requiredFields = currentSection.querySelectorAll('\''input[required]'\'');
      
      let stepValid = true;
      
      requiredFields.forEach(field => {
        const validator = validators[field.id];
        if (!validateField(field.id, validator)) {
          stepValid = false;
          field.focus();
        }
      });
      
      if (!stepValid) {
        currentSection.classList.add('\''shake'\'');
        setTimeout(() => currentSection.classList.remove('\''shake'\''), 500);
        return;
      }
      
      if (currentStep < totalSteps) {
        showStep(currentStep + 1);
      }
    });

    // Test API (optionnel)
    async function testAPI() {
      const apiKey = document.getElementById('\''api_key'\'').value;
      const apiSecret = document.getElementById('\''api_secret'\'').value;
      const apiPassphrase = document.getElementById('\''api_passphrase'\'').value;
      
      if (!apiKey || !apiSecret || !apiPassphrase) {
        return;
      }
      
      const testDiv = document.getElementById('\''apiTest'\'');
      const testResult = document.getElementById('\''testResult'\'');
      
      testDiv.style.display = '\''block'\'';
      testResult.innerHTML = '\''🔍 Test de connexion API...'\'';
      
      try {
        // Simulation d'\''un test API (remplacer par vrai appel si possible)
        await new Promise(resolve => setTimeout(resolve, 2000));
        
        // Ici vous pourriez faire un vrai appel API de test
        // Pour l'\''instant, on simule un succès
        testDiv.className = '\''test-api success'\'';
        testResult.innerHTML = '\''✅ Connexion API réussie!'\'';
        
      } catch (error) {
        testDiv.className = '\''test-api error'\'';
        testResult.innerHTML = '\''❌ Échec de connexion API'\'';
      }
    }

    // Auto-test API quand tous les champs sont remplis
    document.getElementById('\''api_passphrase'\'').addEventListener('\''input'\'', () => {
      const apiKey = document.getElementById('\''api_key'\'').value;
      const apiSecret = document.getElementById('\''api_secret'\'').value;
      const apiPassphrase = document.getElementById('\''api_passphrase'\'').value;
      
      if (apiKey ; apiSecret ; apiPassphrase) {
        setTimeout(testAPI, 1000); // Délai pour éviter trop d'\''appels
      }
    });

    // Soumission du formulaire
    // document.getElementById('\''configForm'\'').addEventListener('\''submit'\'', function(e) {
    //   e.preventDefault();
    //   console.log('\''🔄 [JS] Soumission du formulaire interceptée'\'');
      
    //   // Collecter toutes les données
    //   const formData = new FormData(this);
    //   console.log('\''📝 [JS] Données du formulaire:'\'');
    //   for (let [key, value] of formData.entries()) {
    //     if (key === '\''password'\'' || key === '\''api_secret'\'' || key === '\''api_passphrase'\'') {
    //       console.log(`  ${key}: [MASQUÉ]`);
    //     } else {
    //       console.log(`  ${key}: ${value}`);
    //     }
    //   }
      
    //   // Afficher un message de confirmation
    //   if (confirm('\''Sauvegarder cette configuration et redémarrer le terminal?'\'') {
    //     console.log('\''✅ [JS] Confirmation utilisateur reçue'\'');
    //     // Soumettre le formulaire normalement
    //     console.log('\''📤 [JS] Soumission du formulaire...'\'');
    //     this.submit();
    //   } else {
    //     console.log('\''❌ [JS] Soumission annulée par l'\''utilisateur'\'');
    //   }
    // });

    // Initialisation
    showStep(1);
  </script>
</body>
</html>
  )rawliteral"'\'';

$new = @'\''  String html = "<!DOCTYPE html><html><head><meta charset='\''utf-8'\''><title>Config</title></head><body>";
  html += "<h1>LN Markets Config</h1>";
  html += "<form action='\''/save'\'' method='\''POST'\''>";
  html += "SSID: <input name='\''ssid'\'' required><br>";
  html += "Password: <input name='\''password'\'' type='\''password'\'' required><br>";
  html += "API Key: <input name='\''api_key'\''><br>";
  html += "API Secret: <input name='\''api_secret'\'' type='\''password'\''><br>";
  html += "Passphrase: <input name='\''api_passphrase'\'' type='\''password'\''><br>";
  html += "Lightning: <input name='\''lightning_address'\''><br>";
  html += "Deposit: <input name='\''deposit_lnaddress'\''><br>";
  html += "<button type='\''submit'\''>SAVE</button>";
  html += "</form></body></html>"'\'';

$content -replace $old, $new | Set-Content '\''src\main.cpp'\''
