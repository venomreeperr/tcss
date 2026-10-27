// 1. Select all four buttons and the main body
const buttons = document.querySelectorAll('.button');
const body = document.querySelector('body');

// 2. Loop through each button
buttons.forEach(function (button) {
  // 3. Add a click event listener to each one
  button.addEventListener('click', function (e) {
    // 4. Get the ID of the clicked button (e.g., "grey", "blue")
    const color = e.target.id;

    // 5. Set the body's background color to that ID
    body.style.backgroundColor = color;

    // Bonus: If you click white, change the text to black
    if (color === 'white') {
      body.style.color = '#212121';
    } else {
      body.style.color = 'white'; // Or whatever default text color you want
    }
  });
});